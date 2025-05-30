// The MIT License (MIT)
//
// Copyright (c) 2015 Markus Herb
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
#ifndef KALMAN_EXTENDEDKALMANFILTER_HPP_
#define KALMAN_EXTENDEDKALMANFILTER_HPP_

#include "KalmanFilterBase.hpp"
#include "StandardFilterBase.hpp"
#include "LinearizedSystemModel.hpp"
#include "LinearizedMeasurementModel.hpp"

namespace Kalman {
    
  /**
   * @brief Extended Kalman Filter (EKF)
   * 
   * This implementation is based upon [An Introduction to the Kalman Filter](https://www.cs.unc.edu/~welch/media/pdf/kalman_intro.pdf)
   * by Greg Welch and Gary Bishop.
   *
   * @param StateType The vector-type of the system state (usually some type derived from Kalman::Vector)
   */
  template<class StateType>
  class ExtendedKalmanFilter : public KalmanFilterBase<StateType>,
			       public StandardFilterBase<StateType>
  {
  public:
    //! Kalman Filter base type
    typedef KalmanFilterBase<StateType> KalmanBase;
    //! Standard Filter base type
    typedef StandardFilterBase<StateType> StandardBase;
        
    //! Numeric Scalar Type inherited from base
    using typename KalmanBase::T;
        
    //! State Type inherited from base
    using typename KalmanBase::State;
        
    //! Linearized Measurement Model Type
    template<class Measurement, template<class> class CovarianceBase>
    using MeasurementModelType = LinearizedMeasurementModel<State, Measurement, CovarianceBase>;
        
    //! Linearized System Model Type
    template<class Control, template<class> class CovarianceBase>
    using SystemModelType = LinearizedSystemModel<State, Control, CovarianceBase>;
        
  protected:
    //! Kalman Gain Matrix Type
    template<class Measurement>
    using KalmanGain = Kalman::KalmanGain<State, Measurement>;
        
  protected:
    //! State Estimate
    using KalmanBase::x;
    //! State Covariance Matrix
    using StandardBase::P;
        
  public:
    /**
     * @brief Constructor
     */
    ExtendedKalmanFilter()
    {
      // Setup state and covariance
      P.setIdentity();
    }
        
    /**
     * @brief Perform filter prediction step using system model and no control input (i.e. \f$ u = 0 \f$)
     *
     * @param [in] s The System model
     * @return The updated state estimate
     */
    template<class Control, template<class> class CovarianceBase>
    const State& predict( SystemModelType<Control, CovarianceBase>& s, const float t = 0.05)
    {
      // predict state (without control)
      Control u;
      u.setZero();
      return predict( s, u );
    }
        
    /**
     * @brief Perform filter prediction step using control input \f$u\f$ and corresponding system model
     *
     * @param [in] s The System model
     * @param [in] u The Control input vector
     * @return The updated state estimate
     */
    template<class Control, template<class> class CovarianceBase>
    const State& predict( SystemModelType<Control, CovarianceBase>& s, const Control& u, const T t = 0.05)
    {
      s.updateJacobians(x, u, t);
            
      // predict state
      x = s.f(x, u, t);
            
      // predict covariance
      P  = ( s.F * P * s.F.transpose() ) + ( s.W * s.getCovariance() * s.W.transpose() );


      #ifdef PRINT_DEBUG

      std::cout << "P" << std::endl;
      std::cout << std::endl;
      std::cout << P << std::endl;
      std::cout << std::endl;

      #endif

      // return state prediction
      return this->getState();
    }
        
    /**
     * @brief Perform filter update step using measurement \f$z\f$ and corresponding measurement model
     *
     * @param [in] m The Measurement model
     * @param [in] z The measurement vector
     * @return The updated state estimate
     */
    template<class Measurement, template<class> class CovarianceBase>
    const State& update( MeasurementModelType<Measurement, CovarianceBase>& m, const Measurement& z, const double t = 0.05, const bool mahalanobis = false, const double reject_sigma = 1)
    {

      // std::cout << "KALMAN UPDATING" << std::endl;

      m.updateJacobians( x, t );
            
      // COMPUTE KALMAN GAIN
      // compute innovation covariance
      Covariance<Measurement> S = ( m.H * P * m.H.transpose() ) + ( m.V * m.getCovariance() * m.V.transpose());

      if (mahalanobis) {
	double distance_squared = (z - m.h( x )).transpose() * S.inverse() * (z - m.h( x ));

	if (fabs(distance_squared) > reject_sigma * reject_sigma) {

	  #ifdef PRINT_DEBUG
	  std::cout << std::endl;
	  std::cout << std::endl;
	  std::cout << std::endl;
	  std::cout << std::endl;

	  std::cout
	    << "[WARNING]: IGNORED DUE TO MAHALANOBIS DISTANCE OUTLIER DETECTION"
	    << std::endl;
	  
	  std::cout << std::endl;
	  std::cout << std::endl;
	  std::cout << std::endl;
	  std::cout << std::endl;
	  #endif

	  return this->getState();
	}
      }

      #ifdef PRINT_DEBUG
      
      std::cout << "S = " << std::endl;
      std::cout << std::endl;
      std::cout << S << std::endl;
      std::cout << std::endl;

      std::cout << "P = " << std::endl;
      std::cout << std::endl;
      std::cout << P << std::endl;
      std::cout << std::endl;

      std::cout << "S inverse " << std::endl;
      std::cout << std::endl;
      std::cout << S.inverse() << std::endl;
      std::cout << std::endl;

      #endif
 
      // compute kalman gain
      KalmanGain<Measurement> K = P * m.H.transpose() * S.inverse();

      #ifdef PRINT_DEBUG
      std::cout << "K" << std::endl;
      std::cout << std::endl;
      std::cout << K << std::endl;
      std::cout << std::endl;
      #endif

            
      // UPDATE STATE ESTIMATE AND COVARIANCE
      // Update state using computed kalman gain and innovation
      x += K * ( z - m.h( x ) );

      #ifdef PRINT_DEBUG
      std::cout << "X" << std::endl;
      std::cout << std::endl;
      std::cout << x << std::endl;
      std::cout << std::endl;
      #endif

            
      // Update covariance
      P -= K * m.H * P;


      #ifdef PRINT_DEBUG
      std::cout << "P" << std::endl;
      std::cout << std::endl;
      std::cout << P << std::endl;
      std::cout << std::endl;
      #endif


           
      // return updated state estimate
      return this->getState();
    }

    // /**
    //  * Get current state estimate
    //  */
    // const StandardBase& getCovariance() const
    // {
    //   return P;
    // }

 
  };
}

#endif

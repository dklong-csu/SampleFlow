// ---------------------------------------------------------------------
//
// Copyright (C) 2019 by the SampleFlow authors.
//
// This file is part of the SampleFlow library.
//
// The deal.II library is free software; you can use it, redistribute
// it, and/or modify it under the terms of the GNU Lesser General
// Public License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.
// The full text of the license can be found in the file LICENSE.md at
// the top level directory of deal.II.
//
// ---------------------------------------------------------------------

#ifndef SAMPLEFLOW_CONSUMERS_COVARIANCE_MATRIX_H
#define SAMPLEFLOW_CONSUMERS_COVARIANCE_MATRIX_H

#include <sampleflow/consumer.h>
#include <sampleflow/types.h>
#include <mutex>

#include <boost/numeric/ublas/matrix.hpp>


namespace SampleFlow
{
  namespace Consumers
  {
    /**
     * A Consumer class that implements computing the running covariance matrix
     * $C_k = \frac{1}{k} \sum_{j=1}^k (x_j-{\bar x}_j)(x_j-{\bar x}_j)^T$
     * over all samples seen so far. Here, $x_k$ is the sample and
     * ${\bar x}_k=\frac{1}{k} \sum x_k$ the running mean up to sample $k$.
     * The last value $C_k$ so computed can be obtained by calling the get()
     * function.
     *
     * This class uses the following formula to update the covariance matrix
     * after seeing $k$ samples $x_1\ldots x_k$ (see
     * https://en.wikipedia.org/wiki/Algorithms_for_calculating_variance#Welford's_online_algorithm)
     * and
     * https://en.wikipedia.org/wiki/Algorithms_for_calculating_variance#Online):
     * @f{align*}{
     *      ...
     * @f}
     * This formula can be derived as follows, assuming one also keeps track
     * of the running mean ${\bar x}_k$ as discussed in the MeanValue
     * class:
     * @f{align*}{
     *     C_k &= \frac{1}{k} \sum_{j=1}^k (x_j-{\bar x}_j)(x_j-{\bar x}_j)^T
     *      \\ &= \frac{1}{k} \left[
     *           \sum_{j=1}^{k-1} (x_j-{\bar x}_j)(x_j-{\bar x}_j)^T
     *          +  (x_j-{\bar x}_k)(x_k-{\bar x}_k)^T
     * @f}
     *
     *
     * ### Threading model ###
     *
     * The implementation of this class is thread-safe, i.e., its
     * consume() member function can be called concurrently and from multiple
     * threads.
     *
     *
     * @tparam InputType The C++ type used for the samples $x_k$. In
     *   order to compute covariances, the same kind of requirements
     *   have to hold as listed for the Covariance class.
     */
    template <typename InputType>
    class CovarianceMatrix : public Consumer<InputType>
    {
      public:
        /**
         * The data type of the elements of the input type.
         */
        using scalar_type = typename InputType::value_type;

        /**
         * The type of the information generated by this class, i.e., in which
         * the covariance matrix is computed.
         */
        using value_type = boost::numeric::ublas::matrix<scalar_type>;

        /**
         * Constructor.
         */
        CovarianceMatrix ();

        /**
         * Process one sample by updating the previously computed covariance
         * matrix using this one sample.
         *
         * @param[in] sample The sample to process.
         * @param[in] aux_data Auxiliary data about this sample. The current
         *   class does not know what to do with any such data and consequently
         *   simply ignores it.
         */
        virtual
        void
        consume (InputType     sample,
                 AuxiliaryData aux_data) override;

        /**
         * A function that returns the covariance matrix computed from the
         * samples seen so far. If no samples have been processed so far, then
         * a default-constructed object of type InputType will be returned.
         *
         * @return The computed covariance matrix.
         */
        value_type
        get () const;

      private:
        /**
         * A mutex used to lock access to all member variables when running
         * on multiple threads.
         */
        mutable std::mutex mutex;

        /**
         * The current value of $\bar x_k$ as described in the introduction
         * of this class.
         */
        InputType           current_mean;

        /**
         * The current value of $\bar x_k$ as described in the introduction
         * of this class.
         */
        boost::numeric::ublas::matrix<scalar_type> current_covariance_matrix;

        /**
         * The number of samples processed so far.
         */
        types::sample_index n_samples;
    };



    template <typename InputType>
    CovarianceMatrix<InputType>::
    CovarianceMatrix ()
      :
      n_samples (0)
    {}



    template <typename InputType>
    void
    CovarianceMatrix<InputType>::
    consume (InputType sample, AuxiliaryData /*aux_data*/)
    {
      std::lock_guard<std::mutex> lock(mutex);

      // If this is the first sample we see, initialize the matrix with
      // this sample. After the first sample, the covariance matrix
      // is the zero matrix since a single sample has a zero variance.
      if (n_samples == 0)
        {
          n_samples = 1;
          current_covariance_matrix.resize (sample.size(), sample.size());
          current_mean = std::move(sample);
        }
      else
        {
          // Otherwise update the previously computed covariance by the current
          // sample.
          ++n_samples;

          InputType delta = sample;
          delta -= current_mean;
          for (unsigned int i=0; i<sample.size(); ++i)
            for (unsigned int j=0; j<sample.size(); ++j)
              current_covariance_matrix(i,j) += delta[i]*delta[j]/n_samples;

          // Then also update the running mean:
          InputType update = std::move(sample);
          update -= current_mean;
          update /= n_samples;

          current_mean += update;
        }
    }



    template <typename InputType>
    typename CovarianceMatrix<InputType>::value_type
    CovarianceMatrix<InputType>::
    get () const
    {
      std::lock_guard<std::mutex> lock(mutex);

      return current_covariance_matrix;
    }

  }
}

#endif

#pragma once

#include "wavenet_layer.hpp"

namespace wavenet
{
template <int... values>
using Dilations = std::integer_sequence<int, values...>;

template <typename T,
          int in_size,
          int condition_size,
          int head_size,
          int channels,
          int kernel_size,
          typename DilationsSequence,
          bool has_head_bias,
          typename MathsProvider,
          typename Activation = RTNeural::TanhActivationT<T, channels, MathsProvider>>
struct Layer_Array
{
    template <typename>
    struct Layers_Helper
    {
    };

    template <int... dilation_vals>
    struct Layers_Helper<Dilations<dilation_vals...>>
    {
        using type = std::tuple<Wavenet_Layer<T, condition_size, channels, kernel_size, dilation_vals, MathsProvider, Activation>...>;
    };

    using Layers = typename Layers_Helper<DilationsSequence>::type;

    static constexpr auto n_channels = channels;

    RTNeural::DenseT<T, in_size, channels> rechannel; // no bias!
    Layers layers;
    static constexpr auto num_layers = std::tuple_size_v<decltype (layers)>;
    RTNeural::DenseT<T, channels, head_size, has_head_bias> head_rechannel;

    using Last_Layer_Type = std::remove_reference_t<decltype (std::get<std::tuple_size_v<decltype (layers)> - 1> (layers))>;
    decltype (Last_Layer_Type::outs)& layer_outputs { std::get<std::tuple_size_v<decltype (layers)> - 1> (layers).outs };

#if RTNEURAL_USE_EIGEN
    Eigen::Matrix<T, head_size, 1> head_outputs {};
    Eigen::Matrix<float, channels, 1>* layer_outputs_arr;
    Eigen::Matrix<float, head_size, 1>* head_outputs_arr;
#elif RTNEURAL_USE_XSIMD
    decltype (RTNeural::DenseT<T, channels, head_size>::outs)& head_outputs { head_rechannel.outs };
    using Layer_Out = xsimd::batch<T>[RTNeural::ceil_div (channels, (int) xsimd::batch<T>::size)];
    Layer_Out* layer_outputs_arr;
    using Head_Out = xsimd::batch<T>[RTNeural::ceil_div (head_size, (int) xsimd::batch<T>::size)];
    Head_Out* head_outputs_arr;
#endif

    void reset()
    {
        RTNeural::modelt_detail::forEachInTuple ([] (auto& layer, size_t)
                                                 { layer.reset(); },
                                                 layers);
    }

    static size_t get_arena_bytes_needed (int N)
    {
#if RTNEURAL_USE_EIGEN
        return 2 * sizeof (Eigen::Matrix<float, channels, 1>) * N + sizeof (Eigen::Matrix<float, head_size, 1>) * N;
#elif RTNEURAL_USE_XSIMD
        return 2 * sizeof (Layer_Out) * N + sizeof (Head_Out) * N;
#endif
    }

    void load_weights (std::vector<float>::iterator& weights)
    {
        std::vector<std::vector<float>> rechannel_weights (channels, std::vector<float> (in_size));
        for (int i = 0; i < channels; i++)
            for (int j = 0; j < in_size; j++)
                rechannel_weights[i][j] = *(weights++);
        rechannel.setWeights (rechannel_weights);

        RTNeural::modelt_detail::forEachInTuple ([&weights] (auto& layer, size_t)
                                                 { layer.load_weights (weights); },
                                                 layers);

        std::vector<std::vector<float>> head_rechannel_weights (head_size, std::vector<float> (channels));
        for (int i = 0; i < head_size; i++)
            for (int j = 0; j < channels; j++)
                head_rechannel_weights[i][j] = *(weights++);
        head_rechannel.setWeights (head_rechannel_weights);

        if constexpr (has_head_bias)
        {
            std::vector<float> head_rechannel_bias (head_size);
            for (int i = 0; i < head_size; i++)
                head_rechannel_bias[i] = *(weights++);
            head_rechannel.setBias (head_rechannel_bias.data());
        }
    }

#if RTNEURAL_USE_EIGEN
    void forward (const Eigen::Matrix<T, in_size, 1>& ins,
                  const Eigen::Matrix<T, condition_size, 1>& condition,
                  Eigen::Matrix<T, channels, 1>& head_io)
#elif RTNEURAL_USE_XSIMD
    void forward (const xsimd::batch<T> (&ins)[RTNeural::ceil_div (in_size, (int) xsimd::batch<T>::size)],
                  const xsimd::batch<T> (&condition)[RTNeural::ceil_div (condition_size, (int) xsimd::batch<T>::size)],
                  xsimd::batch<T> (&head_io)[RTNeural::ceil_div (channels, (int) xsimd::batch<T>::size)])
#endif
    {
        rechannel.forward (ins);

        RTNeural::modelt_detail::forEachInTuple (
            [&] (auto& layer, auto index_t)
            {
                static constexpr size_t index = index_t;
                if constexpr (index == 0)
                    layer.forward (rechannel.outs, condition, head_io);
                else
                    layer.forward (std::get<index - 1> (layers).outs, condition, head_io);
            },
            layers);

        head_rechannel.forward (head_io);
#if RTNEURAL_USE_EIGEN
        head_outputs = head_rechannel.outs;
#endif
    }

#if RTNEURAL_USE_EIGEN
    void forward (const Eigen::Matrix<T, in_size, 1>* ins,
                  const Eigen::Matrix<T, condition_size, 1>* condition,
                  Eigen::Matrix<T, channels, 1>* head_io,
                  int N,
                  Memory_Arena<>& arena)
#elif RTNEURAL_USE_XSIMD
    void forward (const xsimd::batch<T> (*ins)[RTNeural::ceil_div (in_size, (int) xsimd::batch<T>::size)],
                  const xsimd::batch<T> (*condition)[RTNeural::ceil_div (condition_size, (int) xsimd::batch<T>::size)],
                  xsimd::batch<T> (*head_io)[RTNeural::ceil_div (channels, (int) xsimd::batch<T>::size)],
                  int N,
                  Memory_Arena<>& arena)
#endif
    {
#if RTNEURAL_USE_EIGEN
        layer_outputs_arr = arena.allocate<Eigen::Matrix<float, channels, 1>> (N, RTNEURAL_DEFAULT_ALIGNMENT);
        head_outputs_arr = arena.allocate<Eigen::Matrix<float, head_size, 1>> (N, RTNEURAL_DEFAULT_ALIGNMENT);
#elif RTNEURAL_USE_XSIMD
        layer_outputs_arr = arena.allocate<Layer_Out> (N, RTNEURAL_DEFAULT_ALIGNMENT);
        head_outputs_arr = arena.allocate<Head_Out> (N, RTNEURAL_DEFAULT_ALIGNMENT);
#endif

        for (int n = 0; n < N; ++n)
        {
            rechannel.forward (ins[n]);
#if RTNEURAL_USE_EIGEN
            layer_outputs_arr[n] = rechannel.outs;
#elif RTNEURAL_USE_XSIMD
            std::copy (std::begin (rechannel.outs), std::end (rechannel.outs), std::begin (layer_outputs_arr[n]));
#endif
        }

        RTNeural::modelt_detail::forEachInTuple (
            [&] (auto& layer, auto)
            {
                layer.forward (layer_outputs_arr, condition, head_io, layer_outputs_arr, N, arena);
            },
            layers);

        for (int n = 0; n < N; ++n)
        {
            head_rechannel.forward (head_io[n]);
#if RTNEURAL_USE_EIGEN
            head_outputs_arr[n] = head_rechannel.outs;
#elif RTNEURAL_USE_XSIMD
            std::copy (std::begin (head_rechannel.outs), std::end (head_rechannel.outs), std::begin (head_outputs_arr[n]));
#endif
        }
    }
};
} // namespace wavenet

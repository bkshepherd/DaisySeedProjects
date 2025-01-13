#pragma once

#include "arena.hpp"
#include "wavenet_layer_array.hpp"

namespace wavenet
{
template <typename T,
          int condition_size,
          typename... LayerArrays>
struct Wavenet_Model
{
    std::tuple<LayerArrays...> layer_arrays;

    static constexpr auto head_layer_n_channels = std::tuple_element_t<0, std::tuple<LayerArrays...>>::n_channels;

#if RTNEURAL_USE_EIGEN
    Eigen::Matrix<T, head_layer_n_channels, 1> head_input {};
#elif RTNEURAL_USE_XSIMD
    xsimd::batch<T> head_input[RTNeural::ceil_div (head_layer_n_channels, (int) xsimd::batch<T>::size)];
#endif
    T head_scale = (T) 0;

    Memory_Arena<> arena {};

    Wavenet_Model() = default;

    void prepare (int block_size)
    {
#if RTNEURAL_USE_EIGEN
        size_t arena_bytes_needed = sizeof (Eigen::Matrix<T, head_layer_n_channels, 1>) * block_size;
#elif RTNEURAL_USE_XSIMD
        size_t arena_bytes_needed = sizeof (xsimd::batch<T>[RTNeural::ceil_div (head_layer_n_channels, (int) xsimd::batch<T>::size)]) * block_size
                                    + sizeof (xsimd::batch<T>) * block_size;
#endif
        RTNeural::modelt_detail::forEachInTuple (
            [&arena_bytes_needed, block_size] (auto& layer_array, auto)
            {
                arena_bytes_needed += layer_array.get_arena_bytes_needed (block_size);
            },
            layer_arrays);

        arena.resize (arena_bytes_needed + 256);
        // prewarm();
    }

    void prewarm()
    {
        RTNeural::modelt_detail::forEachInTuple (
            [] (auto& layer, size_t)
            {
                layer.reset();
            },
            layer_arrays);
        for (int i = 0; i < 1 << 14; ++i)
            forward (0.0f);
    }
    /*
    void load_weights (const nlohmann::json& model_json)
    {
        std::vector<float> model_weights = model_json.at ("weights");
        auto weights_iterator = model_weights.begin();
        RTNeural::modelt_detail::forEachInTuple (
            [&weights_iterator] (auto& layer, size_t)
            {
                layer.load_weights (weights_iterator);
            },
            layer_arrays);

        head_scale = *weights_iterator++;

        // Make sure we use the all of the weights exactly
        assert (std::distance (model_weights.begin(), weights_iterator) == model_weights.size());
    }
    */

    void load_weights (std::vector<float>& model_weights)
    {
        //std::vector<float> model_weights = model_json.at ("weights");
        auto weights_iterator = model_weights.begin();
        RTNeural::modelt_detail::forEachInTuple (
            [&weights_iterator] (auto& layer, size_t)
            {
                layer.load_weights (weights_iterator);
            },
            layer_arrays);

        head_scale = *weights_iterator++;

        // Make sure we use the all of the weights exactly
        int temp = model_weights.size();
        assert (std::distance (model_weights.begin(), weights_iterator) == temp);
        //assert (std::distance (model_weights.begin(), weights_iterator) == model_weights.size());
    }

    T forward (T input) noexcept
    {
#if RTNEURAL_USE_EIGEN
        const auto v_ins = Eigen::Matrix<T, 1, 1>::Constant (input);
#elif RTNEURAL_USE_XSIMD
        xsimd::batch<T> v_ins[1];
        v_ins[0] = RTNeural::set_value (v_ins[0], 0, input);
#endif

        RTNeural::modelt_detail::forEachInTuple (
            [this, v_ins] (auto& layer_array, auto index_t)
            {
                static constexpr size_t index = index_t;
                if constexpr (index == 0)
                {
#if RTNEURAL_USE_EIGEN
                    head_input.setZero();
                    std::get<0> (layer_arrays).forward (v_ins, v_ins, head_input);
#elif RTNEURAL_USE_XSIMD
                    std::fill (std::begin (head_input), std::end (head_input), xsimd::batch<T> {});
                    std::get<0> (layer_arrays).forward (v_ins, v_ins, head_input);
#endif
                }
                else
                {
                    std::get<index> (layer_arrays).forward (std::get<index - 1> (layer_arrays).layer_outputs, v_ins, std::get<index - 1> (layer_arrays).head_outputs);
                }
            },
            layer_arrays);

#if RTNEURAL_USE_EIGEN
        return std::get<std::tuple_size_v<decltype (layer_arrays)> - 1> (layer_arrays).head_outputs[0] * head_scale;
#elif RTNEURAL_USE_XSIMD
        return std::get<std::tuple_size_v<decltype (layer_arrays)> - 1> (layer_arrays).head_outputs[0].get (0) * head_scale;
#endif
    }

    void forward (const T* input, T* output, int N) noexcept
    {
#if RTNEURAL_USE_EIGEN
        const auto* v_ins = reinterpret_cast<const Eigen::Matrix<T, 1, 1>*> (input);
#elif RTNEURAL_USE_XSIMD
        auto* v_ins = arena.allocate<xsimd::batch<T>[1]> (N, RTNEURAL_DEFAULT_ALIGNMENT);
        for (int n = 0; n < N; ++n)
            v_ins[n][0] = RTNeural::set_value (v_ins[n][0], 0, input[n]);
#endif

        RTNeural::modelt_detail::forEachInTuple (
            [this, v_ins, N, output] (auto& layer_array, auto index_t)
            {
                static constexpr size_t index = index_t;
                if constexpr (index == 0)
                {
#if RTNEURAL_USE_EIGEN
                    auto* head_inputs = arena.allocate<Eigen::Matrix<T, head_layer_n_channels, 1>> (N, RTNEURAL_DEFAULT_ALIGNMENT);
                    for (int n = 0; n < N; ++n)
                        head_inputs[n].setZero();
                    std::get<0> (layer_arrays).forward (v_ins, v_ins, head_inputs, N, arena);
#elif RTNEURAL_USE_XSIMD
                    auto* head_inputs = arena.allocate<xsimd::batch<T>[RTNeural::ceil_div (head_layer_n_channels, (int) xsimd::batch<T>::size)]> (N, RTNEURAL_DEFAULT_ALIGNMENT);
                    for (int n = 0; n < N; ++n)
                        std::fill (std::begin (head_inputs[n]), std::end (head_inputs[n]), xsimd::batch<T> {});
                    std::get<0> (layer_arrays).forward (v_ins, v_ins, head_inputs, N, arena);
#endif
                }
                else
                {
                    auto& prev_layer_array = std::get<index - 1> (layer_arrays);
                    std::get<index> (layer_arrays).forward (prev_layer_array.layer_outputs_arr, v_ins, prev_layer_array.head_outputs_arr, N, arena);
                }
            },
            layer_arrays);

        auto& last_layer_array = std::get<std::tuple_size_v<decltype (layer_arrays)> - 1> (layer_arrays);
        for (int n = 0; n < N; ++n)
        {
#if RTNEURAL_USE_EIGEN
            output[n] = last_layer_array.head_outputs_arr[n][0] * head_scale;
#elif RTNEURAL_USE_XSIMD
            output[n] = last_layer_array.head_outputs_arr[n][0].get (0) * head_scale;
#endif
        }

        arena.clear();
    }
};
} // namespace wavenet

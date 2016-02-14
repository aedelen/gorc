#pragma once

#include "content/id.hpp"

namespace gorc {

    template <typename IdT>
    class abstract_component_pool {
    public:
        virtual ~abstract_component_pool()
        {
            return;
        }
    };

}
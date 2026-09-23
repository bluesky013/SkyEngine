//
// Created on 2026/09/23.
//

#include <vegetation/VegetationRenderAdaptor.h>

namespace sky::vegetation {

    void VegetationRenderFactory::Register(Impl *impl)
    {
        factory.reset(impl);
    }

    void VegetationRenderFactory::UnRegister()
    {
        factory.reset();
    }

    IVegetationRenderAdaptor *VegetationRenderFactory::CreateAdaptor()
    {
        return factory ? factory->CreateAdaptor() : nullptr;
    }

} // namespace sky::vegetation

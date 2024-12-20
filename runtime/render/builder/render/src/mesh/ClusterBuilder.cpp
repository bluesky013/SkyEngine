//
// Created by Zach Lee on 2024/12/20.
//

#include <builder/render/mesh/ClusterBuilder.h>
#include <builder/render/mesh/MetisInclude.h>

namespace sky::builder {

    ClusterBuilder::ClusterBuilder()
    {
        int options[METIS_NOPTIONS];
        METIS_SetDefaultOptions(options);
        options[METIS_OPTION_SEED] = 42;
    }

} // namespace sky::builder

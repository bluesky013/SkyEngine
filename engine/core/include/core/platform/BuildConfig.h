//
// Created by Zach Lee on 2026/1/3.
//

#pragma once

#if SKY_EDITOR
    #define EDITABLE(stat) (stat)
#else
    #define EDITABLE(stat)
#endif
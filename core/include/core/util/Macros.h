//
// Created by Zach Lee on 2022/6/21.
//

#pragma once

#define SKY_DISABLE_COPY(className) \
    className(const className&) = delete;            \
    className& operator=(const className&) = delete;
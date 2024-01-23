include(FetchContent)


FetchContent_Declare(
        Ende
        GIT_REPOSITORY https://github.com/olorin99/Ende
        GIT_TAG        a0a2dc20e581e13f55a9acfce6b6b099d989942c
)

FetchContent_Declare(
        Canta
        GIT_REPOSITORY https://github.com/olorin99/Canta
        GIT_TAG "main"
)

FetchContent_MakeAvailable(Ende Canta)
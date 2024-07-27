#pragma once

enum class ApplicationResult
{
    SUCCESS,
    FAILURE,
};

class Application
{
public:
    static auto run() -> ApplicationResult;
};
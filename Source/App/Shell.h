#pragma once

#include "App/WindowControls.h"

namespace Idler
{
    class IdlerModel;

    class Shell : public WindowControls
    {
    public:
        virtual IdlerModel& Model() = 0;
        virtual bool HasTray() const = 0;
        virtual void Restart() = 0;

    protected:
        Shell() = default;
        Shell(const Shell&) = default;
        Shell& operator=(const Shell&) = default;
        ~Shell() = default;
    };
}

#pragma once

namespace Idler
{
    class IdlerModel;

    class Shell
    {
    public:
        virtual IdlerModel& Model() = 0;
        virtual void* NativeWindow() const = 0;

        virtual float CornerRadius() const = 0;
        virtual void AddDragArea(float left, float top, float right, float bottom) = 0;

        virtual void Minimise() = 0;
        virtual bool Maximised() const = 0;
        virtual void ToggleMaximised() = 0;
        virtual float Zoom() const = 0;
        virtual void Close() = 0;
        virtual bool HasTray() const = 0;

    protected:
        Shell() = default;
        Shell(const Shell&) = default;
        Shell& operator=(const Shell&) = default;
        ~Shell() = default;
    };
}

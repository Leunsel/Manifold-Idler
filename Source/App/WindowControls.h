#pragma once

namespace Idler
{
    class WindowControls
    {
    public:
        virtual void* NativeWindow() const = 0;
        virtual float CornerRadius() const = 0;
        virtual void AddDragArea(float left, float top, float right, float bottom) = 0;
        virtual void Minimise() = 0;
        virtual bool Maximised() const = 0;
        virtual void ToggleMaximised() = 0;
        virtual float Zoom() const = 0;
        virtual void Close() = 0;

    protected:
        WindowControls() = default;
        WindowControls(const WindowControls&) = default;
        WindowControls& operator=(const WindowControls&) = default;
        ~WindowControls() = default;
    };
}

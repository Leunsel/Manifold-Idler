#pragma once

namespace Idler::FontData
{
    struct Blob
    {
        const unsigned char* Data = nullptr;
        int Size = 0;
    };

    Blob Text();
    Blob Bold();
    Blob Icons();
}

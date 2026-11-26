#pragma once

namespace cc {

/// Linha de guia (ruler guide) do canvas, em coordenadas de documento.
/// horizontal=true → linha horizontal em y=position; caso contrário vertical
/// em x=position.
struct Guide
{
    bool horizontal = true;
    double position = 0.0;

    bool operator==(const Guide& o) const
    {
        return horizontal == o.horizontal && position == o.position;
    }
    bool operator!=(const Guide& o) const { return !(*this == o); }
};

} // namespace cc

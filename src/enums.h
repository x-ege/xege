#pragma once

namespace ege
{

enum Alignment
{
    Alignment_LEFT    = 0x01,
    Alignment_HMID    = 0x02,
    Alignment_RIGHT   = 0x04,

    Alignment_TOP     = 0x10,
    Alignment_VMID    = 0x20,
    Alignment_BOTTOM  = 0x40,

    Alignment_LEFT_TOP     = Alignment_LEFT  | Alignment_TOP,
    Alignment_LEFT_MID     = Alignment_LEFT  | Alignment_VMID,
    Alignment_LEFT_BOTTOM  = Alignment_LEFT  | Alignment_BOTTOM,

    Alignment_MID_TOP      = Alignment_HMID  | Alignment_TOP,
    Alignment_CENTER       = Alignment_HMID  | Alignment_VMID,
    Alignment_MID_BOTTOM   = Alignment_HMID  | Alignment_BOTTOM,

    Alignment_RIGHT_TOP    = Alignment_RIGHT | Alignment_TOP,
    Alignment_RIGHT_MID    = Alignment_RIGHT | Alignment_VMID,
    Alignment_RIGHT_BOTTOM = Alignment_RIGHT | Alignment_BOTTOM
};

const unsigned int ALIGNMENT_HORIZONTAL_MASK = 0x0F;
const unsigned int ALIGNMENT_VERTICAL_MASK   = 0xF0;

}

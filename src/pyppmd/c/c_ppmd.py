from ._ppmd import Ppmd7Decoder, Ppmd7Encoder, Ppmd8Decoder, Ppmd8Encoder, PPMD8_RESTORE_METHOD_RESTART, PPMD8_RESTORE_METHOD_CUT_OFF

__all__ = (
    "Ppmd7Encoder", "Ppmd7Decoder", "Ppmd8Encoder", "Ppmd8Decoder", "PpmdError",
    "PPMD8_RESTORE_METHOD_RESTART", "PPMD8_RESTORE_METHOD_CUT_OFF",
)

class PpmdError(Exception):
    "Call to the underlying PPMd library failed."
    pass

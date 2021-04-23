from ._ppmd import Ppmd7Decoder, Ppmd7Encoder, Ppmd8Decoder, Ppmd8Encoder

__all__ = ("Ppmd7Encoder", "Ppmd7Decoder", "Ppmd8Encoder", "Ppmd8Decoder", "PpmdError")


class PpmdError(Exception):
    "Call to the underlying PPMd library failed."
    pass

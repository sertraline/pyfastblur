from . import libpyfastblur
import sys
import os

from io import BytesIO
from typing import Union


class InvalidTypeException(Exception):
    def __init__(self):
        message = "fileobj must be a path or non-empty BytesIO PNG file!"
        super().__init__(message)


class InvalidPathException(Exception):
    def __init__(self, filepath):
        message = "Error opening file: '%s' doesn't exist" % filepath
        super().__init__(message)


def blur(fileobj: Union[str, BytesIO], radius, enable_gaussian: bool = False) -> Union[BytesIO, None]:
    """
    :param fileobj: path to file or non-empty BytesIO object with raw PNG bytes
    :param radius: blur radius (higher = stronger)
    :param enable_gaussian: process image with gaussian instead of box blur (slower)
    :return: BytesIO PNG file
    """
    if not type(fileobj) == str and not type(fileobj) == BytesIO:
        raise InvalidTypeException

    if not type(radius) == int:
        raise TypeError("Invalid 'radius' type: must be int")
    if not type(enable_gaussian) == bool:
        raise TypeError("Invalid 'enable_gaussian' type: must be bool")

    if radius < 0:
        radius = (-radius)
    if radius == 0:
        radius = 1

    iobuf = None
    if type(fileobj) == str:
        if not os.path.isfile(fileobj):
            raise InvalidPathException(fileobj)
        with open(fileobj, 'rb') as file:
            iobuf = BytesIO(file.read())
    else:
        if not (type(fileobj) == BytesIO):
            raise InvalidTypeException
        if not (fileobj.getbuffer().nbytes > 0):
            raise InvalidTypeException
        iobuf = fileobj
    out_file = libpyfastblur.blur(radius, iobuf, int(enable_gaussian))
    if not out_file:
        raise TypeError("blur() returned None")
    # output: _io.TextIOWrapper to a file mapped in memory
    # convert to BytesIO object
    out_buf = BytesIO()
    for line in out_file:
        out_buf.write(line)
    out_buf.seek(0)
    return out_buf

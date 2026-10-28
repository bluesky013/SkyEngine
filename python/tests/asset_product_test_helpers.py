"""Encode and check the flat asset envelope used by package fixtures."""

import struct
import uuid


PRODUCT_MAGIC = 0x44525053
PRODUCT_VERSION = 1
PRODUCT_HEADER = struct.Struct('<II16sQQ')


def checksum(body):
    value = 14695981039346656037
    for byte in body:
        value = ((value ^ byte) * 1099511628211) & ((1 << 64) - 1)
    return value


def product_bytes(identity, body):
    return PRODUCT_HEADER.pack(PRODUCT_MAGIC, PRODUCT_VERSION, uuid.UUID(identity).bytes,
                               len(body), checksum(body)) + body


def product_body(data, expected_identity):
    if len(data) < PRODUCT_HEADER.size:
        raise ValueError('Truncated product envelope')
    magic, version, identity, length, expected_checksum = PRODUCT_HEADER.unpack_from(data)
    if magic != PRODUCT_MAGIC or version != PRODUCT_VERSION:
        raise ValueError('Unsupported product envelope')
    if identity != uuid.UUID(expected_identity).bytes:
        raise ValueError('Product identity mismatch')
    body = data[PRODUCT_HEADER.size:]
    if length != len(body) or expected_checksum != checksum(body):
        raise ValueError('Product length or checksum mismatch')
    return body

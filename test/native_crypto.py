"""Build real SHA256/HMAC without Arduino-only Crypto RNG drivers."""

from pathlib import Path

Import("env")


def select_crypto_sources(build_env, node):
    path = Path(node.get_abspath())
    if "Crypto" in path.parts and path.suffix == ".cpp":
        if path.name not in {"Crypto.cpp", "Hash.cpp", "SHA256.cpp"}:
            return None
    return node


env.AddBuildMiddleware(select_crypto_sources)

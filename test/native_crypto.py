"""Build real crypto algorithms without Arduino-only Crypto RNG drivers."""

from pathlib import Path

Import("env")


def select_crypto_sources(build_env, node):
    path = Path(node.get_abspath())
    if "Crypto" in path.parts and path.suffix == ".cpp":
        allowed = {"Crypto.cpp", "Hash.cpp", "SHA256.cpp"}
        if build_env.get("PIOENV") == "native_chat":
            allowed |= {"AES128.cpp", "AESCommon.cpp", "BlockCipher.cpp", "Cipher.cpp",
                        "SHA512.cpp", "Ed25519.cpp", "Curve25519.cpp", "BigNumberUtil.cpp"}
        if path.name not in allowed:
            return None
    return node


env.AddBuildMiddleware(select_crypto_sources)

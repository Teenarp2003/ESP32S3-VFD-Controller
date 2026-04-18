from setuptools import setup

setup(
    name="vfd-ble-cli",
    version="1.0.0",
    py_modules=["ble_terminal"],
    install_requires=["bleak", "prompt_toolkit"],
    entry_points={
        "console_scripts": [
            "vfd=ble_terminal:main",
        ],
    },
)

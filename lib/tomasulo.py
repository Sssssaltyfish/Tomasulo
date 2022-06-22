def _bootstrap():
    global _bootstrap, __loader__, __file__
    import pkg_resources, imp, platform

    if platform.system() == "Windows":
        __file__ = pkg_resources.resource_filename(__name__, "tomasulo.dll")
    else:
        __file__ = pkg_resources.resource_filename(__name__, "tomasulo.so")
    __loader__ = None
    del _bootstrap, __loader__
    imp.load_dynamic(__name__, __file__)


_bootstrap()

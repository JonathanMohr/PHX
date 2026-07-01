from ci.defs import BuildContext, BuildMode

from typing import Callable, TypeAlias
from pathlib import Path
import shutil

class Toolchain:
    # self, mode, src, src_rel, out_dir, doCompileCommands -> object
    Compile_Function: TypeAlias = Callable[["Toolchain", BuildMode, Path, Path, Path, bool], Path]
    # self, mode, objects, name, out_dir -> lib
    Archive_Function: TypeAlias = Callable[["Toolchain", BuildMode, list[Path], str, Path, bool], Path]
    # self, mode, objects, libraries, name, out_dir -> executable, debug_info | None
    Link_Executable_Function: TypeAlias = Callable[["Toolchain", BuildMode, list[Path], list[Path], str, Path], tuple[Path, Path | None]]
    # self, mode, objects, libraries, name, out_dir, plugin -> dylib, implib | None, debug_info | None
    Link_DynamicLibrary_Function: TypeAlias = Callable[["Toolchain", BuildMode, list[Path], list[Path], str, Path, bool], tuple[Path, Path | None, Path | None]]

    _Compile_C_Source: Compile_Function
    _Compile_CPP_Source: Compile_Function
    _Archive_Objects: Archive_Function
    _Link_Executable: Link_Executable_Function
    _Link_DynamicLibrary: Link_DynamicLibrary_Function

    def __init__(self, context: BuildContext,
                 Compile_C_Source: Compile_Function,
                 Compile_CPP_Source: Compile_Function,
                 Archive_Objects: Archive_Function,
                 Link_Executable: Link_Executable_Function,
                 Link_DynamicLibrary: Link_DynamicLibrary_Function):
        self.context = context

        self.defines: list[tuple[str, str]] = []
        self.includeDirs: list[Path] = []
        self.libraryDirs: list[Path] = []
        self.libraries: list[str] = []

        self.stdc: str = "c99"
        self.stdcpp: str = "c++98"

        self._Compile_C_Source = Compile_C_Source
        self._Compile_CPP_Source = Compile_CPP_Source
        self._Archive_Objects = Archive_Objects
        self._Link_Executable = Link_Executable
        self._Link_DynamicLibrary = Link_DynamicLibrary

    def Set_STDC(self, stdc: str):
        self.stdc = stdc

    def Set_STDCPP(self, stdcpp: str):
        self.stdcpp = stdcpp

    def Add_Define(self, name: str, value: str | None = None):
        self.defines.append((name, value))

    def Add_Include_Directory(self, dir: Path):
        self.includeDirs.append(dir)

    def Add_Library_Directory(self, dir: Path):
        self.libraryDirs.append(dir)

    def Add_Library(self, lib: str):
        self.libraries.append(lib)

    
    def Compile_C_Source(self, mode: BuildMode, src: Path, src_rel: Path, out_dir: Path, doCompileCommands: bool) -> Path:
        return self._Compile_C_Source(self, mode, src, src_rel, out_dir, doCompileCommands)
    
    def Compile_CPP_Source(self, mode: BuildMode, src: Path, src_rel: Path, out_dir: Path, doCompileCommands: bool) -> Path:
        return self._Compile_CPP_Source(self, mode, src, src_rel, out_dir, doCompileCommands)
    
    def Archive_Objects(self, mode: BuildMode, objects: list[Path], name: str, out_dir: Path) -> Path:
        return self._Archive_Objects(self, mode, objects, name, out_dir)
    
    def Link_Executable(self, mode: BuildMode, objects: list[Path], libraries: list[Path], name: str, out_dir: Path) -> tuple[Path, Path | None]:
        return self._Link_Executable(self, mode, objects, libraries, name, out_dir)
    
    def Link_DynamicLibrary(self, mode: BuildMode, objects: list[Path], libraries: list[Path], name: str, out_dir: Path, plugin: bool) -> tuple[Path, Path | None, Path | None]:
        return self._Link_DynamicLibrary(self, mode, objects, libraries, name, out_dir, plugin)

class ToolchainError(RuntimeError):
    pass

def Require_Tool(name: str) -> str:
    path = shutil.which(name)
    if not path:
        raise ToolchainError(f"Missing required tool: {name}")
    return path

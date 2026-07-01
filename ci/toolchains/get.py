from ci.defs import BuildContext
from ci.toolchains.toolchain import Toolchain
import ci.toolchains.llvm as llvm

def Get_LLVM_Toolchain(context: BuildContext) -> Toolchain:
    toolchain = Toolchain(
        context,
        llvm.Compile_C_Source,
        llvm.Compile_CPP_Source,
        llvm.Archive_Objects,
        llvm.Link_Executable,
        llvm.Link_DynamicLibrary
    )

    return toolchain

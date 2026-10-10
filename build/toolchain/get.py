from build.defs import BuildContext
from build.toolchain.toolchain import Toolchain
import build.toolchain.llvm as llvm
import build.toolchain.nasm as nasm

def Get_LLVM_Toolchain(context: BuildContext) -> Toolchain:
    toolchain = Toolchain(
        context,
        llvm.Compile_C_Source,
        llvm.Compile_CPP_Source,
        nasm.Compile_Assembly_Source,
        llvm.Archive_Objects,
        llvm.Link_Executable,
        llvm.Link_DynamicLibrary
    )

    return toolchain

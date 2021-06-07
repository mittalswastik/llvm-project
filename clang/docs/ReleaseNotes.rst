========================================
Clang 13.0.0 (In-Progress) Release Notes
========================================

.. contents::
   :local:
   :depth: 2

Written by the `LLVM Team <https://llvm.org/>`_

.. warning::

   These are in-progress notes for the upcoming Clang 13 release.
   Release notes for previous releases can be found on
   `the Download Page <https://releases.llvm.org/download.html>`_.

Introduction
============

This document contains the release notes for the Clang C/C++/Objective-C
frontend, part of the LLVM Compiler Infrastructure, release 13.0.0. Here we
describe the status of Clang in some detail, including major
improvements from the previous release and new feature work. For the
general LLVM release notes, see `the LLVM
documentation <https://llvm.org/docs/ReleaseNotes.html>`_. All LLVM
releases may be downloaded from the `LLVM releases web
site <https://llvm.org/releases/>`_.

For more information about Clang or LLVM, including information about the
latest release, please see the `Clang Web Site <https://clang.llvm.org>`_ or the
`LLVM Web Site <https://llvm.org>`_.

Note that if you are reading this file from a Git checkout or the
main Clang web page, this document applies to the *next* release, not
the current one. To see the release notes for a specific release, please
see the `releases page <https://llvm.org/releases/>`_.

What's New in Clang 13.0.0?
===========================

Some of the major new features and improvements to Clang are listed
here. Generic improvements to Clang as a whole or to its underlying
infrastructure are described first, followed by language-specific
sections with improvements to Clang's support for those languages.

Major New Features
------------------

- Guaranteed tail calls are now supported with statement attributes
  ``[[clang::musttail]]`` in C++ and ``__attribute__((musttail))`` in C. The
  attribute is applied to a return statement (not a function declaration),
  and an error is emitted if a tail call cannot be guaranteed, for example if
  the function signatures of caller and callee are not compatible. Guaranteed
  tail calls enable a class of algorithms that would otherwise use an
  arbitrary amount of stack space.

Improvements to Clang's diagnostics
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- ...

Non-comprehensive list of changes in this release
-------------------------------------------------

- The default value of _MSC_VER was raised from 1911 to 1914. MSVC 19.14 has the
  support to overaligned objects on x86_32 which is required for some LLVM 
  passes.

New Compiler Flags
------------------

- ``-Wreserved-identifier`` emits warning when user code uses reserved
  identifiers.

<<<<<<< HEAD
- ``-fstack-usage`` generates an extra .su file per input source file. The .su
  file contains frame size information for each function defined in the source
  file.
=======
- AArch64 options ``-moutline-atomics``, ``-mno-outline-atomics`` to enable
  and disable calls to helper functions implementing atomic operations. These
  out-of-line helpers like '__aarch64_cas8_relax' will detect at runtime
  AArch64 Large System Extensions (LSE) availability and either use their
  atomic instructions, or falls back to LL/SC loop. These options do not apply
  if the compilation target supports LSE. Atomic instructions are used directly
  in that case. The option's behaviour mirrors GCC, the helpers are implemented
  both in compiler-rt and libgcc.

- New option ``-fbinutils-version=`` specifies the targeted binutils version.
  For example, ``-fbinutils-version=2.35`` means compatibility with GNU as/ld
  before 2.35 is not needed: new features can be used and there is no need to
  work around old GNU as/ld bugs.
>>>>>>> 0826268d59c6e1bb3530dffd9dc5f6038774486d

Deprecated Compiler Flags
-------------------------

<<<<<<< HEAD
=======
The following options are deprecated and ignored. They will be removed in
future versions of Clang.

- The clang-cl ``/fallback`` flag, which made clang-cl invoke Microsoft Visual
  C++ on files it couldn't compile itself, has been deprecated. It will be
  removed in Clang 13.

>>>>>>> 0826268d59c6e1bb3530dffd9dc5f6038774486d
- ...

Modified Compiler Flags
-----------------------

<<<<<<< HEAD
- -Wshadow now also checks for shadowed structured bindings
- ``-B <prefix>`` (when ``<prefix>`` is a directory) was overloaded to additionally
  detect GCC installations under ``<prefix>`` (``lib{,32,64}/gcc{,-cross}/$triple``).
  This behavior was incompatible with GCC, caused interop issues with
  ``--gcc-toolchain``, and was thus dropped. Specify ``--gcc-toolchain=<dir>``
  instead. ``-B``'s other GCC-compatible semantics are preserved:
  ``$prefix/$triple-$file`` and ``$prefix$file`` are searched for executables,
  libraries, includes, and data files used by the compiler.
=======
- On ELF, ``-gz`` now defaults to ``-gz=zlib`` with the integrated assembler.
  It produces ``SHF_COMPRESSED`` style compression of debug information. GNU
  binutils 2.26 or newer, or lld is required to link produced object files. Use
  ``-gz=zlib-gnu`` to get the old behavior.
- Now that `this` pointers are tagged with `nonnull` and `dereferenceable(N)`,
  `-fno-delete-null-pointer-checks` has gained the power to remove the
  `nonnull` attribute on `this` for configurations that need it to be nullable.
- ``-gsplit-dwarf`` no longer implies ``-g2``.
- ``-fasynchronous-unwind-tables`` is now the default on Linux AArch64/PowerPC.
  This behavior matches newer GCC.
  (`D91760 <https://reviews.llvm.org/D91760>`_)
  (`D92054 <https://reviews.llvm.org/D92054>`_)
- Support has been added for the following processors (command-line identifiers
  in parentheses):

  - Arm Cortex-A78C (cortex-a78c).
  - Arm Cortex-R82 (cortex-r82).
  - Arm Neoverse V1 (neoverse-v1).
  - Arm Neoverse N2 (neoverse-n2).
  - Fujitsu A64FX (a64fx).
  For example, to select architecture support and tuning for Neoverse-V1 based
  systems, use ``-mcpu=neoverse-v1``.
>>>>>>> 0826268d59c6e1bb3530dffd9dc5f6038774486d

Removed Compiler Flags
-------------------------

- The clang-cl ``/fallback`` flag, which made clang-cl invoke Microsoft Visual
  C++ on files it couldn't compile itself, has been removed.

- ``-Wreturn-std-move-in-c++11``, which checked whether an entity is affected by
  `CWG1579 <https://wg21.link/CWG1579>`_ to become implicitly movable, has been
  removed.

New Pragmas in Clang
--------------------

- ...

Modified Pragmas in Clang
-------------------------

- The "#pragma clang loop vectorize_width" has been extended to support an
  optional 'fixed|scalable' argument, which can be used to indicate that the
  compiler should use fixed-width or scalable vectorization.  Fixed-width is
  assumed by default.

  Scalable or vector length agnostic vectorization is an experimental feature
  for targets that support scalable vectors. For more information please refer
  to the Clang Language Extensions documentation.

Attribute Changes in Clang
--------------------------

- ...

- Added support for C++11-style ``[[]]`` attributes on using-declarations, as a
  clang extension.

Windows Support
---------------

- Implicitly add ``.exe`` suffix for MinGW targets, even when cross compiling.
  (This matches a change from GCC 8.)

- Windows on Arm64: programs using the C standard library's setjmp and longjmp
  functions may crash with a "Security check failure or stack buffer overrun"
  exception. To workaround (with reduced security), compile with
  /guard:cf,nolongjmp.

- Windows on Arm64: LLVM 12 adds official binary release hosted on
  Windows on Arm64.  The binary is built and tested by Linaro alongside
  AArch64 and ARM 32-bit Linux binary releases.  This first WoA release
  includes Clang compiler, LLD Linker, and compiler-rt runtime libraries.
  Work on LLDB, sanitizer support, OpenMP, and other features is in progress
  and will be included in future Windows on Arm64 LLVM releases.

C Language Changes in Clang
---------------------------

- ...

C++ Language Changes in Clang
-----------------------------

- The oldest supported GNU libstdc++ is now 4.8.3 (released 2014-05-22).
  Clang workarounds for bugs in earlier versions have been removed.

- ...

C++20 Feature Support
^^^^^^^^^^^^^^^^^^^^^
...

C++2b Feature Support
^^^^^^^^^^^^^^^^^^^^^
...

Objective-C Language Changes in Clang
-------------------------------------

OpenCL Kernel Language Changes in Clang
---------------------------------------

- Improved online documentation: :doc:`UsersManual` and :doc:`OpenCLSupport`
  pages.
- Added ``-cl-std=CL3.0`` and predefined version macro for OpenCL 3.0.
- Added ``-cl-std=CL1.0`` and mapped to the existing OpenCL 1.0 functionality.
- Improved OpenCL extension handling per target.
- Added clang extension for function pointers ``__cl_clang_function_pointers``
  and variadic functions ``__cl_clang_variadic_functions``, more details can be
  found in :doc:`LanguageExtensions`.
- Removed extensions without kernel language changes:
  ``cl_khr_select_fprounding_mode``, ``cl_khr_gl_sharing``, ``cl_khr_icd``,
  ``cl_khr_gl_event``, ``cl_khr_d3d10_sharing``, ``cl_khr_context_abort``,
  ``cl_khr_d3d11_sharing``, ``cl_khr_dx9_media_sharing``,
  ``cl_khr_image2d_from_buffer``, ``cl_khr_initialize_memory``,
  ``cl_khr_gl_depth_images``, ``cl_khr_spir``, ``cl_khr_egl_event``,
  ``cl_khr_egl_image``, ``cl_khr_terminate_context``.
- Improved diagnostics for  unevaluated ``vec_step`` expression.
- Allow nested pointers (e.g. pointer-to-pointer) kernel arguments beyond OpenCL
  1.2.
- Added ``global_device`` and ``global_host`` address spaces for USM
  allocations.

Miscellaneous improvements in C++ for OpenCL support:

- Added diagnostics for pointers to member functions and references to
  functions.
- Added support of ``vec_step`` builtin.
- Fixed ICE on address spaces with forwarding references and templated copy
  constructors.
- Removed warning for variadic macro use.

ABI Changes in Clang
--------------------

OpenMP Support in Clang
-----------------------

- ...

CUDA Support in Clang
---------------------

- ...

X86 Support in Clang
--------------------

- ...

Internal API Changes
--------------------

These are major API changes that have happened since the 12.0.0 release of
Clang. If upgrading an external codebase that uses Clang as a library,
this section should help get you past the largest hurdles of upgrading.

- ...

Build System Changes
--------------------

These are major changes to the build system that have happened since the 12.0.0
release of Clang. Users of the build system should adjust accordingly.

- The option ``LIBCLANG_INCLUDE_CLANG_TOOLS_EXTRA`` no longer exists. There were
  two releases with that flag forced off, and no uses were added that forced it
  on. The recommended replacement is clangd.

- ...

AST Matchers
------------

<<<<<<< HEAD
- ...
=======
- The ``mapAnyOf()`` matcher was added. This allows convenient matching of
  different AST nodes which have a compatible matcher API. For example,
  ``mapAnyOf(ifStmt, forStmt).with(hasCondition(integerLiteral()))``
  matches any ``IfStmt`` or ``ForStmt`` with a integer literal as the
  condition.

- The ``binaryOperation()`` matcher allows matching expressions which
  appear like binary operators in the code, even if they are really
  ``CXXOperatorCallExpr`` for example. It is based on the ``mapAnyOf()``
  matcher functionality. The matcher API for the latter node has been
  extended with ``hasLHS()`` etc to facilitate the abstraction.

- Matcher API for ``CXXRewrittenBinaryOperator`` has been added. In addition
  to explicit matching with the ``cxxRewrittenBinaryOperator()`` matcher, the
  ``binaryOperation()`` matches on nodes of this type.

- The behavior of ``TK_IgnoreUnlessSpelledInSource`` with the ``traverse()``
  matcher has been changed to no longer match on template instantiations or on
  implicit nodes which are not spelled in the source.

- The ``TK_IgnoreImplicitCastsAndParentheses`` traversal kind was removed. It
  is recommended to use ``TK_IgnoreUnlessSpelledInSource`` instead.

- The behavior of the ``forEach()`` matcher was changed to not internally
  ignore implicit and parenthesis nodes.  This makes it consistent with
  the ``has()`` matcher.  Uses of ``forEach()`` relying on the old behavior
  can now use the  ``traverse()`` matcher or ``ignoringParenCasts()``.

- Several AST Matchers have been changed to match based on the active
  traversal mode.  For example, ``argumentCountIs()`` matches the number of
  arguments written in the source, ignoring default arguments represented
  by ``CXXDefaultArgExpr`` nodes.

- Improvements in AST Matchers allow more matching of template declarations,
  independent of their template instantations.
>>>>>>> 0826268d59c6e1bb3530dffd9dc5f6038774486d

clang-format
------------

- Option ``SpacesInLineCommentPrefix`` has been added to control the
  number of spaces in a line comments prefix.

- Option ``SortIncludes`` has been updated from a ``bool`` to an
  ``enum`` with backwards compatibility. In addition to the previous
  ``true``/``false`` states (now ``CaseSensitive``/``Never``), a third
  state has been added (``CaseInsensitive``) which causes an alphabetical sort
  with case used as a tie-breaker.

  .. code-block:: c++

    // Never (previously false)
    #include "B/A.h"
    #include "A/B.h"
    #include "a/b.h"
    #include "A/b.h"
    #include "B/a.h"

    // CaseSensitive (previously true)
    #include "A/B.h"
    #include "A/b.h"
    #include "B/A.h"
    #include "B/a.h"
    #include "a/b.h"

    // CaseInsensitive
    #include "A/B.h"
    #include "A/b.h"
    #include "a/b.h"
    #include "B/A.h"
    #include "B/a.h"

- ``BasedOnStyle: InheritParentConfig`` allows to use the ``.clang-format`` of
  the parent directories to overwrite only parts of it.

- Option ``IndentAccessModifiers`` has been added to be able to give access
  modifiers their own indentation level inside records.

- Option ``PPIndentWidth`` has been added to be able to configure pre-processor
  indentation independent from regular code.

- Option ``ShortNamespaceLines`` has been added to give better control
  over ``FixNamespaceComments`` when determining a namespace length.

- Support for Whitesmiths has been improved, with fixes for ``namespace`` blocks
  and ``case`` blocks and labels.

- Option ``EmptyLineAfterAccessModifier`` has been added to remove, force or keep
  new lines after access modifiers.

- Checks for newlines in option ``EmptyLineBeforeAccessModifier`` are now based
  on the formatted new lines and not on the new lines in the file. (Fixes
  https://llvm.org/PR41870.)

- Option ``SpacesInAngles`` has been improved, it now accepts ``Leave`` value
  that allows to keep spaces where they are already present.

- Option ``AllowShortIfStatementsOnASingleLine`` has been improved, it now
  accepts ``AllIfsAndElse`` value that allows to put "else if" and "else" short
  statements on a single line. (Fixes https://llvm.org/PR50019.)

- Option ``BreakInheritanceList`` gets a new style, ``AfterComma``. It breaks
  only after the commas that separate the base-specifiers.

- ``git-clang-format`` no longer formats changes to symbolic links. (Fixes
  https://llvm.org/PR46992.)

- Makes ``PointerAligment: Right`` working with ``AlignConsecutiveDeclarations``.
  (Fixes https://llvm.org/PR27353)

libclang
--------

- ...

Static Analyzer
---------------

.. 3ff220de9009 [analyzer][StdLibraryFunctionsChecker] Add POSIX networking functions
.. ...And a million other patches.
- Improve the analyzer's understanding of several POSIX functions.

.. https://reviews.llvm.org/D86533#2238207
- Greatly improved the analyzer’s constraint solver by better understanding
  when constraints are imposed on multiple symbolic values that are known to be
  equal or known to be non-equal. It will now also efficiently reject impossible
  if-branches between known comparison expressions. (Incorrectly stated as a
  11.0.0 feature in the previous release notes)

.. 820e8d8656ec [Analyzer][WebKit] UncountedLambdaCaptureChecker
- New checker: :ref:`webkit.UncountedLambdaCapturesChecker<webkit-UncountedLambdaCapturesChecker>`
  is a WebKit coding convention checker that flags raw pointers to
  reference-counted objects captured by lambdas and suggests using intrusive
  reference-counting smart pointers instead.

.. 8a64689e264c [Analyzer][WebKit] UncountedLocalVarsChecker
- New checker: :ref:`alpha.webkit.UncountedLocalVarsChecker<alpha-webkit-UncountedLocalVarsChecker>`
  is a WebKit coding convention checker that intends to make sure that any
  uncounted local variable is backed by a ref-counted object with lifetime that
  is strictly larger than the scope of the uncounted local variable.

.. i914f6c4ff8a4 [StaticAnalyzer] Support struct annotations in FuchsiaHandleChecker
- ``fuchia.HandleChecker`` now recognizes handles in structs; All the handles
  referenced by the structure (direct value or ptr) would be treated as
  containing the release/use/acquire annotations directly.

.. 8deaec122ec6 [analyzer] Update Fuchsia checker to catch releasing unowned handles.
- Fuchsia checkers can detect the release of an unowned handle.

- Numerous fixes and improvements to bug report generation.

.. _release-notes-ubsan:

Undefined Behavior Sanitizer (UBSan)
------------------------------------

Core Analysis Improvements
==========================

- ...

New Issues Found
================

- ...

Python Binding Changes
----------------------

The following methods have been added:

-  ...

Significant Known Problems
==========================

Additional Information
======================

A wide variety of additional information is available on the `Clang web
page <https://clang.llvm.org/>`_. The web page contains versions of the
API documentation which are up-to-date with the Git version of
the source code. You can access versions of these documents specific to
this release by going into the "``clang/docs/``" directory in the Clang
tree.

If you have any questions or comments about Clang, please feel free to
contact us via the `mailing
list <https://lists.llvm.org/mailman/listinfo/cfe-dev>`_.

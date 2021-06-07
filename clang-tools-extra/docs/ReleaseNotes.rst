====================================================
Extra Clang Tools 13.0.0 (In-Progress) Release Notes
====================================================

.. contents::
   :local:
   :depth: 3

Written by the `LLVM Team <https://llvm.org/>`_

.. warning::

   These are in-progress notes for the upcoming Extra Clang Tools 13 release.
   Release notes for previous releases can be found on
   `the Download Page <https://releases.llvm.org/download.html>`_.

Introduction
============

This document contains the release notes for the Extra Clang Tools, part of the
Clang release 13.0.0. Here we describe the status of the Extra Clang Tools in
some detail, including major improvements from the previous release and new
feature work. All LLVM releases may be downloaded from the `LLVM releases web
site <https://llvm.org/releases/>`_.

For more information about Clang or LLVM, including information about
the latest release, please see the `Clang Web Site <https://clang.llvm.org>`_ or
the `LLVM Web Site <https://llvm.org>`_.

Note that if you are reading this file from a Git checkout or the
main Clang web page, this document applies to the *next* release, not
the current one. To see the release notes for a specific release, please
see the `releases page <https://llvm.org/releases/>`_.

What's New in Extra Clang Tools 13.0.0?
=======================================

Some of the major new features and improvements to Extra Clang Tools are listed
here. Generic improvements to Extra Clang Tools as a whole or to its underlying
infrastructure are described first, followed by tool-specific sections.

Major New Features
------------------

...

Improvements to clangd
----------------------

<<<<<<< HEAD
The improvements are...
=======
Performance
^^^^^^^^^^^

- clangd's memory usage is significantly reduced on most Linux systems.
  In particular, memory usage should not increase dramatically over time.

  The standard allocator on most systems is glibc's ptmalloc2, and it creates
  disproportionately large heaps when handling clangd's allocation patterns.
  By default, clangd will now periodically call ``malloc_trim`` to release free
  pages on glibc systems.

  Users of other allocators (such as ``jemalloc`` or ``tcmalloc``) on glibc
  systems can disable this using ``--malloc_trim=0`` or the CMake flag
  ``-DCLANGD_MALLOC_TRIM=0``.
>>>>>>> 0826268d59c6e1bb3530dffd9dc5f6038774486d

- Added the `$/memoryUsage request
  <https://clangd.llvm.org/extensions.html#memory-usage>`_: an LSP extension.
  This provides a breakdown of the memory clangd thinks it is using (excluding
  malloc overhead etc). The clangd VSCode extension supports showing the memory
  usage tree.

Parsing and selection
^^^^^^^^^^^^^^^^^^^^^

- Improved navigation of broken code in C using Recovery AST. (This has been
  enabled for C++ since clangd 11).

- Types are understood more often in broken code. (This is the first release
  where Recovery AST preserves speculated types).

- Heuristic resolution for dependent names in templates.

Code completion
^^^^^^^^^^^^^^^

- Higher priority for symbols that were already used in this file, and symbols
  from namespaces mentioned in this file. (Estimated 3% accuracy improvement)

- Introduced a ranking algorithm trained on snippets from a large C++ codebase.
  Use the flag ``--ranking-model=decision_forest`` to try this (Estimated 6%
  accuracy improvement). This mode is likely to become the default in future.

  Note: this is a generic model, not specialized for your code. clangd does not
  collect any data from your code to train code completion.

- Signature help works with functions with template-dependent parameter types.

Go to definition
^^^^^^^^^^^^^^^^

- Selecting an ``auto`` or ``decltype`` keyword will attempt to navigate to
  a definition of the deduced type.

- Improved handling of aliases: navigate to the underlying entity more often.

- Better understanding of declaration vs definition for Objective-C classes and
  protocols.

- Selecting a pure-virtual method shows its overrides.

Find references
^^^^^^^^^^^^^^^

- Indexes are smarter about not returning stale references when code is deleted.

- References in implementation files are always indexed, so results should be
  more complete.

- Find-references on a virtual method shows references to overridden methods.

New navigation features
^^^^^^^^^^^^^^^^^^^^^^^

- Call hierarchy (``textDocument/callHierarchy``) is supported.
  Only incoming calls are available.

- Go to implementation (``textDocument/implementation``) is supported on
  abstract classes, and on virtual methods.

- Symbol search (``workspace/symbol``) queries may be partially qualified.
  That is, typing ``b::Foo`` will match the symbol ``a::b::c::Foo``.

Refactoring
^^^^^^^^^^^

- New refactoring: populate ``switch`` statement with cases.
  (This acts as a fix for the ``-Wswitch-enum`` warning).

- Renaming templates is supported, and many other complex cases were fixed.

- Attempting to rename to an invalid or conflicting name can produce an error
  message rather than broken code. (Not all cases are detected!)

- The accuracy of many code actions has been improved.

Hover
^^^^^

- Hovers for ``auto`` and ``decltype`` show the type in the same style as other
  hovers. ``this`` is also now supported.

- Displayed type names are more consistent and idiomatic.

Semantic highlighting
^^^^^^^^^^^^^^^^^^^^^

- Inactive preprocessor regions (``#ifdef``) are highlighted as comments.

- clangd 12 is the last release with support for the non-standard
  ``textDocument/semanticHighlights`` notification. Clients sholud migrate to
  the ``textDocument/semanticTokens`` request added in LSP 3.16.

Remote index (alpha)
^^^^^^^^^^^^^^^^^^^^

- clangd can now connect to a remote index server instead of building a project
  index locally. This saves resources in large codebases that are slow to index.

- The server program is ``clangd-index-server``, and it consumes index files
  produced by ``clangd-indexer``.

- This feature requires clangd to be built with the CMake flag
  ``-DCLANGD_ENABLE_REMOTE=On``, which requires GRPC libraries and is not
  enabled by default. Unofficial releases of the remote-index-enabled client
  and server tools are at https://github.com/clangd/clangd/releases

- Large projects can deploy a shared server, and check in a ``.clangd`` file
  to enable it (in the ``Index.External`` section). We hope to provide such a
  server for ``llvm-project`` itself in the near future.

Configuration
^^^^^^^^^^^^^

- Static and remote indexes can be configured in the ``Index.External`` section.
  Different static indexes can now be used for different files.
  (Obsoletes the flag ``--index-file``).

- Diagnostics can be filtered or suppressed in the ``Diagnostics`` section.

- Clang-tidy checks can be enabled/disabled in the ``Diagnostics.ClangTidy``
  section. (Obsoletes the flag ``--clang-tidy-checks``).

- The compilation database directory can be configured in the ``CompileFlags``
  section. Different compilation databases can now be specified for different
  files. (Obsoletes the flag ``--compile-commands-dir``).

- Errors in loaded configuration files are published as LSP diagnostics, and so
  should be shown in your editor.

`Full reference of configuration options <https://clangd.llvm.org/config.html>`_

System integration
^^^^^^^^^^^^^^^^^^

- Changes to ``compile_commands.json`` and ``compile_flags.txt`` will take
  effect the next time a file is parsed, without restarting clangd.

- ``clangd --check=<filename>`` can be run on the command-line to simulate
  opening a file without actually using an editor. This can be useful to
  reproduce crashes or aother problems.

- Various fixes to handle filenames correctly (and case-insensitively) on
  windows.

- If incoming LSP messages are malformed, the logs now contain details.

Miscellaneous
^^^^^^^^^^^^^

- "Show AST" request
  (`textDocument/ast <https://clangd.llvm.org/extensions.html#ast>`_)
  added as an LSP extension. This displays a simplified view of the clang AST
  for selected code. The clangd VSCode extension supports this.

- clangd should no longer crash while loading old or corrupt index files.

- The flags ``--index``, ``--recovery-ast`` and ``-suggest-missing-includes``
  have been retired. These features are now always enabled.

- Too many stability and correctness fixes to mention.

Improvements to clang-doc
-------------------------

The improvements are...

Improvements to clang-query
---------------------------

The improvements are...

Improvements to clang-rename
----------------------------

The improvements are...

Improvements to clang-tidy
--------------------------

- The `run-clang-tidy.py` helper script is now installed in `bin/` as
  `run-clang-tidy`. It was previously installed in `share/clang/`.

- Added command line option `--fix-notes` to apply fixes found in notes
  attached to warnings. These are typically cases where we are less confident
  the fix will have the desired effect.

- libToolingCore and Clang-Tidy was refactored and now checks can produce
  highlights (`^~~~~` under fragments of the source code) in diagnostics.
  Existing and new checks in the future can be expected to start implementing
  this functionality.
  This change only affects the visual rendering of diagnostics, and does not
  alter the behavior of generated fixes.

New checks
^^^^^^^^^^

- New :doc:`bugprone-implicit-widening-of-multiplication-result
  <clang-tidy/checks/bugprone-implicit-widening-of-multiplication-result>` check.

  Diagnoses instances of an implicit widening of multiplication result.

- New :doc:`concurrency-thread-canceltype-asynchronous
  <clang-tidy/checks/concurrency-thread-canceltype-asynchronous>` check.

  Finds ``pthread_setcanceltype`` function calls where a thread's cancellation
  type is set to asynchronous.

- New :doc:`altera-id-dependent-backward-branch
  <clang-tidy/checks/altera-id-dependent-backward-branch>` check.

  Finds ID-dependent variables and fields that are used within loops. This
  causes branches to occur inside the loops, and thus leads to performance
  degradation.

- New :doc:`altera-unroll-loops
  <clang-tidy/checks/altera-unroll-loops>` check.

  Finds inner loops that have not been unrolled, as well as fully unrolled
  loops with unknown loops bounds or a large number of iterations.

<<<<<<< HEAD
- New :doc:`cppcoreguidelines-prefer-member-initializer
  <clang-tidy/checks/cppcoreguidelines-prefer-member-initializer>` check.

  Finds member initializations in the constructor body which can be placed into
  the initialization list instead.

- New :doc:`bugprone-unhandled-exception-at-new
  <clang-tidy/checks/bugprone-unhandled-exception-at-new>` check.
=======
- New :doc:`bugprone-misplaced-pointer-arithmetic-in-alloc
  <clang-tidy/checks/bugprone-misplaced-pointer-arithmetic-in-alloc>` check.

- New :doc:`bugprone-redundant-branch-condition
  <clang-tidy/checks/bugprone-redundant-branch-condition>` check.
>>>>>>> 0826268d59c6e1bb3530dffd9dc5f6038774486d

  Finds calls to ``new`` with missing exception handler for ``std::bad_alloc``.

New check aliases
^^^^^^^^^^^^^^^^^

- New alias :doc:`cert-pos47-c
  <clang-tidy/checks/cert-pos47-c>` to
  :doc:`concurrency-thread-canceltype-asynchronous
  <clang-tidy/checks/concurrency-thread-canceltype-asynchronous>` was added.

Changes in existing checks
^^^^^^^^^^^^^^^^^^^^^^^^^^

- Improved :doc:`bugprone-signal-handler
  <clang-tidy/checks/bugprone-signal-handler>` check.

  Added an option to choose the set of allowed functions.

- Improved :doc:`readability-uniqueptr-delete-release
  <clang-tidy/checks/readability-uniqueptr-delete-release>` check.

  Added an option to choose whether to refactor by calling the ``reset`` member
  function or assignment to ``nullptr``.
  Added support for pointers to ``std::unique_ptr``.

Removed checks
^^^^^^^^^^^^^^

- The readability-deleted-default check has been removed.
  
  The clang warning `Wdefaulted-function-deleted
  <https://clang.llvm.org/docs/DiagnosticsReference.html#wdefaulted-function-deleted>`_
  will diagnose the same issues and is enabled by default.

Deprecated checks
^^^^^^^^^^^^^^^^^

- The :doc:`readability-deleted-default
  <clang-tidy/checks/readability-deleted-default>` check has been deprecated.
  
  The clang warning `Wdefaulted-function-deleted
  <https://clang.llvm.org/docs/DiagnosticsReference.html#wdefaulted-function-deleted>`_
  will diagnose the same issues and is enabled by default.

Improvements to include-fixer
-----------------------------

The improvements are...

Improvements to clang-include-fixer
-----------------------------------

The improvements are...

Improvements to modularize
--------------------------

The improvements are...

Improvements to pp-trace
------------------------

The improvements are...

Clang-tidy visual studio plugin
-------------------------------

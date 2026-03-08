/*
 @licstart  The following is the entire license notice for the JavaScript code in this file.

 The MIT License (MIT)

 Copyright (C) 1997-2020 by Dimitri van Heesch

 Permission is hereby granted, free of charge, to any person obtaining a copy of this software
 and associated documentation files (the "Software"), to deal in the Software without restriction,
 including without limitation the rights to use, copy, modify, merge, publish, distribute,
 sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in all copies or
 substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
 BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

 @licend  The above is the entire license notice for the JavaScript code in this file
*/
var NAVTREE =
[
  [ "Beast JSON", "index.html", [
    [ "Beast JSON — Technical Reference (v1.0)", "index.html", "index" ],
    [ "Beast JSON — Getting Started", "md_docs_2_g_e_t_t_i_n_g___s_t_a_r_t_e_d.html", [
      [ "Requirements", "md_docs_2_g_e_t_t_i_n_g___s_t_a_r_t_e_d.html#autotoc_md50", null ],
      [ "Installation", "md_docs_2_g_e_t_t_i_n_g___s_t_a_r_t_e_d.html#autotoc_md52", [
        [ "Option A: Single Header Drop-in", "md_docs_2_g_e_t_t_i_n_g___s_t_a_r_t_e_d.html#autotoc_md53", null ],
        [ "Option B: CMake FetchContent", "md_docs_2_g_e_t_t_i_n_g___s_t_a_r_t_e_d.html#autotoc_md54", null ],
        [ "Option C: Clone &amp; Build", "md_docs_2_g_e_t_t_i_n_g___s_t_a_r_t_e_d.html#autotoc_md55", null ]
      ] ],
      [ "First Parse", "md_docs_2_g_e_t_t_i_n_g___s_t_a_r_t_e_d.html#autotoc_md57", null ],
      [ "Reading Values", "md_docs_2_g_e_t_t_i_n_g___s_t_a_r_t_e_d.html#autotoc_md59", [
        [ "Type-checked access (throws on mismatch)", "md_docs_2_g_e_t_t_i_n_g___s_t_a_r_t_e_d.html#autotoc_md60", null ],
        [ "Implicit conversion (nlohmann-style)", "md_docs_2_g_e_t_t_i_n_g___s_t_a_r_t_e_d.html#autotoc_md61", null ],
        [ "Non-throwing access (<span class=\"tt\">try_as</span>)", "md_docs_2_g_e_t_t_i_n_g___s_t_a_r_t_e_d.html#autotoc_md62", null ],
        [ "Pipe fallback (default values, never throws)", "md_docs_2_g_e_t_t_i_n_g___s_t_a_r_t_e_d.html#autotoc_md63", null ],
        [ "Nested access", "md_docs_2_g_e_t_t_i_n_g___s_t_a_r_t_e_d.html#autotoc_md64", null ]
      ] ],
      [ "Safe Access Patterns", "md_docs_2_g_e_t_t_i_n_g___s_t_a_r_t_e_d.html#autotoc_md66", [
        [ "find() — returns optional&lt;Value&gt;", "md_docs_2_g_e_t_t_i_n_g___s_t_a_r_t_e_d.html#autotoc_md67", null ],
        [ "SafeValue — optional-propagating proxy", "md_docs_2_g_e_t_t_i_n_g___s_t_a_r_t_e_d.html#autotoc_md68", null ],
        [ "contains() and value()", "md_docs_2_g_e_t_t_i_n_g___s_t_a_r_t_e_d.html#autotoc_md69", null ]
      ] ],
      [ "Iterating Objects and Arrays", "md_docs_2_g_e_t_t_i_n_g___s_t_a_r_t_e_d.html#autotoc_md71", [
        [ "Object iteration", "md_docs_2_g_e_t_t_i_n_g___s_t_a_r_t_e_d.html#autotoc_md72", null ],
        [ "Array iteration", "md_docs_2_g_e_t_t_i_n_g___s_t_a_r_t_e_d.html#autotoc_md73", null ],
        [ "C++20 Ranges pipelines", "md_docs_2_g_e_t_t_i_n_g___s_t_a_r_t_e_d.html#autotoc_md74", null ]
      ] ],
      [ "Mutating Documents", "md_docs_2_g_e_t_t_i_n_g___s_t_a_r_t_e_d.html#autotoc_md76", [
        [ "Value mutation (scalar overlay)", "md_docs_2_g_e_t_t_i_n_g___s_t_a_r_t_e_d.html#autotoc_md77", null ],
        [ "Structural mutation (add / remove)", "md_docs_2_g_e_t_t_i_n_g___s_t_a_r_t_e_d.html#autotoc_md78", null ]
      ] ],
      [ "Serialization", "md_docs_2_g_e_t_t_i_n_g___s_t_a_r_t_e_d.html#autotoc_md80", [
        [ "dump() — allocates a new string", "md_docs_2_g_e_t_t_i_n_g___s_t_a_r_t_e_d.html#autotoc_md81", null ],
        [ "dump(out) — reuse existing buffer", "md_docs_2_g_e_t_t_i_n_g___s_t_a_r_t_e_d.html#autotoc_md82", null ]
      ] ],
      [ "Auto-Serialization (Structs)", "md_docs_2_g_e_t_t_i_n_g___s_t_a_r_t_e_d.html#autotoc_md84", [
        [ "Serializing Third-Party Types (ADL)", "md_docs_2_g_e_t_t_i_n_g___s_t_a_r_t_e_d.html#autotoc_md86", null ]
      ] ],
      [ "RFC 8259 Strict Mode", "md_docs_2_g_e_t_t_i_n_g___s_t_a_r_t_e_d.html#autotoc_md88", null ],
      [ "Buffer Reuse for Hot Loops", "md_docs_2_g_e_t_t_i_n_g___s_t_a_r_t_e_d.html#autotoc_md90", null ],
      [ "Build Options Reference", "md_docs_2_g_e_t_t_i_n_g___s_t_a_r_t_e_d.html#autotoc_md92", null ],
      [ "Running Tests", "md_docs_2_g_e_t_t_i_n_g___s_t_a_r_t_e_d.html#autotoc_md94", null ],
      [ "Language Bindings", "md_docs_2_g_e_t_t_i_n_g___s_t_a_r_t_e_d.html#autotoc_md96", [
        [ "C API", "md_docs_2_g_e_t_t_i_n_g___s_t_a_r_t_e_d.html#autotoc_md97", null ],
        [ "Python", "md_docs_2_g_e_t_t_i_n_g___s_t_a_r_t_e_d.html#autotoc_md98", null ]
      ] ],
      [ "Common Pitfalls", "md_docs_2_g_e_t_t_i_n_g___s_t_a_r_t_e_d.html#autotoc_md100", [
        [ "1. Document must outlive all Value objects", "md_docs_2_g_e_t_t_i_n_g___s_t_a_r_t_e_d.html#autotoc_md101", null ],
        [ "2. string_view source must remain alive", "md_docs_2_g_e_t_t_i_n_g___s_t_a_r_t_e_d.html#autotoc_md102", null ],
        [ "3. Reusing a Document clears all mutations", "md_docs_2_g_e_t_t_i_n_g___s_t_a_r_t_e_d.html#autotoc_md103", null ],
        [ "4. int ambiguity — use size_t for array index", "md_docs_2_g_e_t_t_i_n_g___s_t_a_r_t_e_d.html#autotoc_md104", null ],
        [ "5. as&lt;int&gt; on a double returns a cast, not an error", "md_docs_2_g_e_t_t_i_n_g___s_t_a_r_t_e_d.html#autotoc_md105", null ]
      ] ]
    ] ],
    [ "Namespaces", "namespaces.html", [
      [ "Namespace List", "namespaces.html", "namespaces_dup" ],
      [ "Namespace Members", "namespacemembers.html", [
        [ "All", "namespacemembers.html", null ],
        [ "Functions", "namespacemembers_func.html", null ],
        [ "Variables", "namespacemembers_vars.html", null ],
        [ "Typedefs", "namespacemembers_type.html", null ],
        [ "Enumerations", "namespacemembers_enum.html", null ],
        [ "Enumerator", "namespacemembers_eval.html", null ]
      ] ]
    ] ],
    [ "Concepts", "concepts.html", "concepts" ],
    [ "Classes", "annotated.html", [
      [ "Class List", "annotated.html", "annotated_dup" ],
      [ "Class Index", "classes.html", null ],
      [ "Class Hierarchy", "hierarchy.html", "hierarchy" ],
      [ "Class Members", "functions.html", [
        [ "All", "functions.html", "functions_dup" ],
        [ "Functions", "functions_func.html", "functions_func" ],
        [ "Variables", "functions_vars.html", null ],
        [ "Typedefs", "functions_type.html", null ],
        [ "Related Symbols", "functions_rela.html", null ]
      ] ]
    ] ],
    [ "Files", "files.html", [
      [ "File List", "files.html", "files_dup" ],
      [ "File Members", "globals.html", [
        [ "All", "globals.html", null ],
        [ "Macros", "globals_defs.html", null ]
      ] ]
    ] ]
  ] ]
];

var NAVTREEINDEX =
[
"annotated.html",
"classbeast_1_1json_1_1lazy_1_1_value.html#a6df8845bcbb7e56865ae42d0827f598b",
"index.html#autotoc_md10",
"structbeast_1_1json_1_1lazy_1_1_value_1_1_key_iterator.html#aa9aa3f748b9dd5032e38820a7cfb730f"
];

var SYNCONMSG = 'click to disable panel synchronization';
var SYNCOFFMSG = 'click to enable panel synchronization';
var LISTOFALLMEMBERS = 'List of all members';
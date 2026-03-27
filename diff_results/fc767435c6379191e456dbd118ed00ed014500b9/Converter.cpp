--- /tmp/tmp.QMgVR8dB4J/old_file.cpp	2026-03-27 17:06:16.894795415 +0900
+++ /tmp/tmp.QMgVR8dB4J/new_file.cpp	2026-03-27 17:06:16.895795374 +0900
@@ -354,6 +354,7 @@
   }
 
   // Output assert_error block if reach_error was called
+  // This self-loop is necessary for the T2 parser to correctly parse the file
   if (m_reachErrorCalled && m_t2Output) {
     std::cout << "FROM: assert_error;" << std::endl;
     std::cout << "TO: assert_error;" << std::endl;
@@ -957,8 +958,8 @@
     m_rules.push_back(rule);
 
     if (!m_phase1 && m_t2Output) {
-      std::cout << "START: " << (bb->getName().str()) << ";" << std::endl
-                << std::endl;
+      std::cout << "START: " << (bb->getName().str()) << ";" << std::endl;
+      std::cout << "ERROR: assert_error;" << std::endl << std::endl;
 
       //<Negar>
       // if (hasUnreachableBlock) {
@@ -2594,14 +2595,20 @@
           m_blockRules.push_back(rule2);
         }
         if (callee->isDeclaration()) {
-          // Special handling for reach_error()
-          if (callee->getName() == "reach_error") {
-            if (m_t2Output) {
-              // Create a transition to assert_error block
+          // Special handling for error functions (reach_error, abort,
+          // __VERIFIER_error, etc.)
+          std::string funcName = callee->getName().str();
+          if (funcName == "reach_error" || funcName == "abort" ||
+              funcName == "__VERIFIER_error" || funcName == "__assert_fail") {
+            if (m_t2Output && !m_currentBlockReachErrorCalled) {
+              // Create a transition to assert_error block (only once per block)
               std::cout << "TO: assert_error;" << std::endl;
               std::cout << std::endl;
               m_currentBlockReachErrorCalled = true; // For current block
               m_reachErrorCalled = true;             // For global tracking
+            } else if (!m_t2Output) {
+              // For non-T2 output, still track the call
+              m_reachErrorCalled = true;
             }
             // Continue with normal non-returning behavior
           }

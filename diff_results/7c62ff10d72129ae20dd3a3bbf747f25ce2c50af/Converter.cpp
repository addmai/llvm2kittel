--- /tmp/tmp.QMgVR8dB4J/old_file.cpp	2026-03-27 17:06:16.543810159 +0900
+++ /tmp/tmp.QMgVR8dB4J/new_file.cpp	2026-03-27 17:06:16.447814191 +0900
@@ -351,6 +351,13 @@
     ref<Rule> rule = Rule::create(lhs, rhs, Constraint::_true);
     m_rules.push_back(rule);
   }
+
+  // Output assert_error block if reach_error was called
+  if (reachErrorCalled && m_t2Output) {
+    std::cout << "FROM: assert_error;" << std::endl;
+    std::cout << "TO: assert_error;" << std::endl;
+    std::cout << std::endl;
+  }
 }
 
 std::list<ref<Rule>> Converter::getRules() { return m_rules; }
@@ -1210,7 +1217,7 @@
         // Later when visiting terminating transition
         // will print transition from true to BB
         llvm::BasicBlock *iBlock = branch->getSuccessor(0);
-        if (m_t2Output) {
+        if (m_t2Output && !reachErrorCalled) {
           std::cout << "TO: " << (iBlock->getName().str()) << ";" << std::endl;
         }
       } else {
@@ -1226,7 +1233,7 @@
           exit(1);
         }
 
-        if (m_t2Output) {
+        if (m_t2Output && !reachErrorCalled) {
           // Given then we're branching, create an intermediate node to branch
           // through via condition.
           std::cout << "TO: " << (pBlock->getName().str()) << "_end;"
@@ -2580,6 +2587,16 @@
           m_blockRules.push_back(rule2);
         }
         if (callee->isDeclaration()) {
+          // Special handling for reach_error()
+          if (callee->getName() == "reach_error") {
+            if (m_t2Output) {
+              // Create a transition to assert_error block
+              std::cout << "TO: assert_error;" << std::endl;
+              std::cout << std::endl;
+              reachErrorCalled = true;
+            }
+            // Continue with normal non-returning behavior
+          }
           // they don't mess with globals
           continue;
         }

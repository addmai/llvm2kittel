--- /tmp/tmp.QMgVR8dB4J/old_file.cpp	2026-03-27 17:06:17.672762738 +0900
+++ /tmp/tmp.QMgVR8dB4J/new_file.cpp	2026-03-27 17:06:17.746759630 +0900
@@ -3060,6 +3060,22 @@
             [&basePoint](const GetElementPtrInst &getElementPtrInst) {
               return getElementPtrInst.variable == "v" + basePoint;
             });
+        if (it == getElementPtrInsts.end()) {
+          // Fallback: we failed to track the originating pointer value.
+          // Generate a conservative assignment to keep the translation sound
+          // instead of crashing (observed when Ptr2Arr introduces additional
+          // pointer temporaries without names).
+          if (!varName.empty()) {
+            std::cout << "v" << varName << " := nondet();" << std::endl;
+          }
+          return;
+        }
+        if (it == getElementPtrInsts.end()) {
+          if (!varName.empty()) {
+            std::cout << "v" << varName << " := nondet();" << std::endl;
+          }
+          return;
+        }
         std::string arrayName = it->array;
         std::string index = it->index;
         llvm::Value *offsetOperand =

--- /tmp/tmp.QMgVR8dB4J/old_file.cpp	2026-03-27 17:06:17.986749549 +0900
+++ /tmp/tmp.QMgVR8dB4J/new_file.cpp	2026-03-27 17:06:18.068746105 +0900
@@ -307,14 +307,13 @@
         } else {
           // Define an array -> Globally -> One-dimensional -> 1
           std::string arrayName = "v" + varName;
-          // if (globalVar.hasInitializer() &&
-          // llvm::isa<llvm::ConstantAggregateZero>(const_cast<llvm::Constant
-          // *>(globalVar.getInitializer()))) {
-          //     globalInsts.push_back(arrayName + " := const_array(0);");
-          // }
-          // else {
-          globalInsts.push_back("v" + varName + " := nondet();");
-          //}
+          if (GV.hasInitializer() &&
+              llvm::isa<llvm::ConstantAggregateZero>(
+                  const_cast<llvm::Constant *>(GV.getInitializer()))) {
+            globalInsts.push_back(arrayName + " := const_array(0);");
+          } else {
+            globalInsts.push_back("v" + varName + " := nondet();");
+          }
           if (std::find(oneDimArrs.begin(), oneDimArrs.end(), arrayName) ==
               oneDimArrs.end()) {
             oneDimArrs.push_back(arrayName);

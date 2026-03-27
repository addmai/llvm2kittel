--- /tmp/tmp.QMgVR8dB4J/old_file.cpp	2026-03-27 17:06:17.395774372 +0900
+++ /tmp/tmp.QMgVR8dB4J/new_file.cpp	2026-03-27 17:06:17.362775758 +0900
@@ -224,6 +224,14 @@
              "False: jump back to the same basic block where unreachable was "
              "encountered (non-termination)."),
     cl::init(true)); // Default is true (termination)
+static cl::opt<bool> ignoreReachError(
+    "ignore-reach-error",
+    cl::desc(
+        "Ignore reach_error() calls completely (useful for termination "
+        "analysis). "
+        "True: treat reach_error() as empty function. "
+        "False: generate ERROR state and transitions (for safety analysis)."),
+    cl::init(false)); // Default is false (safety analysis)
 //</Negar>
 
 void transformModule(llvm::Module *module, llvm::Function *function,
@@ -981,12 +989,12 @@
       // onlyMultiPredIsControl, boundedIntegers, unsignedEncoding,
       // onlyLoopConditions, divisionConstraintType, bitwiseConditions,
       // complexityTuples || uniformComplexityTuples, t2Output);
-      Converter converter(boolType, assumeIsControl, selectIsControl,
-                          onlyMultiPredIsControl, boundedIntegers,
-                          unsignedEncoding, onlyLoopConditions,
-                          divisionConstraintType, bitwiseConditions,
-                          complexityTuples || uniformComplexityTuples, t2Output,
-                          signednessInfo, nondetTypeInfo, unreachableExit);
+      Converter converter(
+          boolType, assumeIsControl, selectIsControl, onlyMultiPredIsControl,
+          boundedIntegers, unsignedEncoding, onlyLoopConditions,
+          divisionConstraintType, bitwiseConditions,
+          complexityTuples || uniformComplexityTuples, t2Output, signednessInfo,
+          nondetTypeInfo, unreachableExit, ignoreReachError);
       //</Negar>
       std::map<llvm::Function *, MayMustMap>::iterator tmp1 = mmMap.find(curr);
       if (tmp1 == mmMap.end()) {

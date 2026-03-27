--- /tmp/tmp.QMgVR8dB4J/old_file.cpp	2026-03-27 17:06:17.098786847 +0900
+++ /tmp/tmp.QMgVR8dB4J/new_file.cpp	2026-03-27 17:06:17.100786763 +0900
@@ -11,6 +11,9 @@
 #include "llvm2kittel/IntTRS/Term.h"
 #include "llvm2kittel/Util/Version.h"
 
+// C++ includes
+#include <algorithm>
+
 //<Negar>
 #include <llvm/Support/raw_ostream.h>
 //</Negar>
@@ -94,7 +97,8 @@
                      DivRemConstraintType divisionConstraintType,
                      bool bitwiseConditions, bool complexityTuples,
                      const bool t2Output, bool signednessInfo,
-                     bool nondetTypeInfo, bool unreachableExit)
+                     bool nondetTypeInfo, bool unreachableExit,
+                     bool ignoreReachError)
     : m_entryBlock(NULL), m_boolType(boolType), m_blockRules(), m_rules(),
       m_vars(), m_lhs(), m_counter(0), m_phase1(true), m_globals(), m_mmMap(),
       m_funcMayZap(), m_tfMap(), m_elcMap(), m_returns(), m_idMap(), m_phiMap(),
@@ -108,10 +112,28 @@
       m_bitwiseConditions(bitwiseConditions),
       m_complexityTuples(complexityTuples), m_complexityLHSs(),
       m_t2Output(t2Output), m_reachErrorCalled(false),
-      m_currentBlockReachErrorCalled(false), signednessInfo(signednessInfo),
+      m_currentBlockReachErrorCalled(false),
+      m_ignoreReachError(ignoreReachError), signednessInfo(signednessInfo),
       nondetTypeInfo(nondetTypeInfo), unreachableExit(unreachableExit) {}
 //</Negar>
 
+// Helper function to ensure assume condition is valid
+// If condition is just "nondet()", convert it to "nondet() != 0"
+std::string ensureValidAssumeCondition(const std::string &condStr) {
+  // Check if the condition is just "nondet()" (possibly with whitespace)
+  std::string trimmed = condStr;
+  trimmed.erase(0, trimmed.find_first_not_of(" \t\n\r"));
+  trimmed.erase(trimmed.find_last_not_of(" \t\n\r") + 1);
+
+  if (trimmed == "nondet()" || trimmed.find("nondet()") == 0) {
+    // If it's just nondet() with no comparison, add != 0
+    if (trimmed == "nondet()") {
+      return "nondet() != 0";
+    }
+  }
+  return condStr;
+}
+
 bool Converter::isTrivial(void) {
   llvm::SmallVector<
       std::pair<const llvm::BasicBlock *, const llvm::BasicBlock *>,
@@ -223,13 +245,22 @@
           if (phiNode->getType() == m_boolType ||
               !phiNode->getType()->isIntegerTy()) {
             std::string variable = phiNode->getName().str();
+            // Remove dots from variable names to avoid syntax errors in T2
+            // format
+            variable.erase(std::remove(variable.begin(), variable.end(), '.'),
+                           variable.end());
             for (unsigned i = 0; i < phiNode->getNumIncomingValues(); ++i) {
               PhiInst phiInst;
               phiInst.variable = "v" + variable;
               llvm::Value *value = phiNode->getIncomingValue(i);
               std::string valueStr;
               if (value->hasName()) {
-                valueStr = "v" + value->getName().str();
+                valueStr = value->getName().str();
+                // Remove dots from variable names
+                valueStr.erase(
+                    std::remove(valueStr.begin(), valueStr.end(), '.'),
+                    valueStr.end());
+                valueStr = "v" + valueStr;
               } else {
                 llvm::raw_string_ostream rso(valueStr);
                 value->print(rso);
@@ -354,12 +385,12 @@
   }
 
   // Output assert_error block if reach_error was called
-  // This self-loop is necessary for the T2 parser to correctly parse the file
-  if (m_reachErrorCalled && m_t2Output) {
-    std::cout << "FROM: assert_error;" << std::endl;
-    std::cout << "TO: assert_error;" << std::endl;
-    std::cout << std::endl;
-  }
+  // Self-loop removed: CoAR now protects error states during LTS simplification
+  // if (m_reachErrorCalled && m_t2Output) {
+  //   std::cout << "FROM: assert_error;" << std::endl;
+  //   std::cout << "TO: assert_error;" << std::endl;
+  //   std::cout << std::endl;
+  // }
 }
 
 std::list<ref<Rule>> Converter::getRules() { return m_rules; }
@@ -421,6 +452,8 @@
 
 std::string Converter::getVar(llvm::Value *V) {
   std::string name = V->getName();
+  // Remove dots from variable names to avoid syntax errors in T2 format
+  name.erase(std::remove(name.begin(), name.end(), '.'), name.end());
   if (!name.empty() && name[0] == '\'') {
     name = name.substr(1);
   } else {
@@ -959,7 +992,9 @@
 
     if (!m_phase1 && m_t2Output) {
       std::cout << "START: " << (bb->getName().str()) << ";" << std::endl;
-      std::cout << "ERROR: assert_error;" << std::endl << std::endl;
+      if (!m_ignoreReachError) {
+        std::cout << "ERROR: assert_error;" << std::endl << std::endl;
+      }
 
       //<Negar>
       // if (hasUnreachableBlock) {
@@ -1159,8 +1194,14 @@
                                  return phiInst.basicBlock == basicBlock;
                                });
       if (myIt != phiInsts.end()) {
-        std::cout << "var__temp_" << myIt->variable << " := " << myIt->value
-                  << ";" << std::endl;
+        // Ensure the value is valid (convert nondet() to nondet() != 0 if
+        // needed)
+        std::string value = myIt->value;
+        if (value == "nondet()") {
+          value = "nondet() != 0";
+        }
+        std::cout << "var__temp_" << myIt->variable << " := " << value << ";"
+                  << std::endl;
       }
       //</Negar>
 
@@ -1299,7 +1340,9 @@
               // Transition where the condition holds
               std::cout << "FROM: " << (pBlock->getName().str()) << "_end;"
                         << std::endl;
-              std::cout << "assume(" << (c->toT2String()) << ");" << std::endl;
+              std::cout << "assume("
+                        << ensureValidAssumeCondition(c->toT2String()) << ");"
+                        << std::endl;
               llvm::BasicBlock *tBlock = branch->getSuccessor(0);
               std::cout << "TO: " << (tBlock->getName().str()) << ";"
                         << std::endl
@@ -1308,8 +1351,10 @@
               // Transition where the condition doesn't hold.
               std::cout << "FROM: " << (pBlock->getName().str()) << "_end;"
                         << std::endl;
-              std::cout << "assume(" << ((c->toNNF(true))->toT2String()) << ");"
-                        << std::endl;
+              std::cout << "assume("
+                        << ensureValidAssumeCondition(
+                               (c->toNNF(true))->toT2String())
+                        << ");" << std::endl;
               llvm::BasicBlock *fBlock = branch->getSuccessor(1);
               std::cout << "TO: " << (fBlock->getName().str()) << ";"
                         << std::endl;
@@ -1318,7 +1363,9 @@
             // Transition where the condition holds
             std::cout << "FROM: " << (pBlock->getName().str()) << "_end;"
                       << std::endl;
-            std::cout << "assume(" << (c->toT2String()) << ");" << std::endl;
+            std::cout << "assume("
+                      << ensureValidAssumeCondition(c->toT2String()) << ");"
+                      << std::endl;
             llvm::BasicBlock *tBlock = branch->getSuccessor(0);
             std::cout << "TO: " << (tBlock->getName().str()) << ";" << std::endl
                       << std::endl;
@@ -1326,8 +1373,10 @@
             // Transition where the condition doesn't hold.
             std::cout << "FROM: " << (pBlock->getName().str()) << "_end;"
                       << std::endl;
-            std::cout << "assume(" << ((c->toNNF(true))->toT2String()) << ");"
-                      << std::endl;
+            std::cout << "assume("
+                      << ensureValidAssumeCondition(
+                             (c->toNNF(true))->toT2String())
+                      << ");" << std::endl;
             llvm::BasicBlock *fBlock = branch->getSuccessor(1);
             std::cout << "TO: " << (fBlock->getName().str()) << ";"
                       << std::endl;
@@ -2523,6 +2572,11 @@
         if (CF->getIntrinsicID() == llvm::Intrinsic::memcpy) {
           std::string leftVar = I.getOperand(0)->getName().str();
           std::string rightVar = I.getOperand(1)->getName().str();
+          // Remove dots from variable names to avoid syntax errors in T2 format
+          leftVar.erase(std::remove(leftVar.begin(), leftVar.end(), '.'),
+                        leftVar.end());
+          rightVar.erase(std::remove(rightVar.begin(), rightVar.end(), '.'),
+                         rightVar.end());
           std::cout << "v" << leftVar << " := v" << rightVar << ";"
                     << std::endl;
         }
@@ -2600,7 +2654,11 @@
           std::string funcName = callee->getName().str();
           if (funcName == "reach_error" || funcName == "abort" ||
               funcName == "__VERIFIER_error" || funcName == "__assert_fail") {
-            if (m_t2Output && !m_currentBlockReachErrorCalled) {
+            if (m_ignoreReachError) {
+              // For termination analysis: treat reach_error() as empty function
+              // Don't generate ERROR state or transitions
+              // Just continue as if the function does nothing
+            } else if (m_t2Output && !m_currentBlockReachErrorCalled) {
               // Create a transition to assert_error block (only once per block)
               std::cout << "TO: assert_error;" << std::endl;
               std::cout << std::endl;
@@ -2732,7 +2790,8 @@
       // Branch to assignment where condition is true.
       std::cout << "FROM: " << (pBlock->getName().str()) << "_" << (getVar(&I))
                 << ";" << std::endl;
-      std::cout << "assume(" << (c->toT2String()) << ");" << std::endl;
+      std::cout << "assume(" << ensureValidAssumeCondition(c->toT2String())
+                << ");" << std::endl;
       std::cout << (getVar(&I))
                 << " := " << (getPolynomial(I.getTrueValue()))->toString()
                 << ";" << std::endl;
@@ -2743,8 +2802,9 @@
       // Branch to assignment where condition is false.
       std::cout << "FROM: " << (pBlock->getName().str()) << "_" << (getVar(&I))
                 << ";" << std::endl;
-      std::cout << "assume(" << ((c->toNNF(true))->toT2String()) << ");"
-                << std::endl;
+      std::cout << "assume("
+                << ensureValidAssumeCondition((c->toNNF(true))->toT2String())
+                << ");" << std::endl;
       std::cout << (getVar(&I))
                 << " := " << (getPolynomial(I.getFalseValue()))->toString()
                 << ";" << std::endl;
@@ -2809,9 +2869,15 @@
       // Access an element of an array -> One-dimensional -> Fixed-index OR
       // Variable-index
       std::string arrayName = gepInst.getPointerOperand()->getName().str();
-      if (arrayName.find(".str") == std::string::npos &&
-          arrayName.find("__PRETTY_FUNCTION__.") == std::string::npos) {
+      // Remove dots from array names
+      arrayName.erase(std::remove(arrayName.begin(), arrayName.end(), '.'),
+                      arrayName.end());
+      if (arrayName.find("str") == std::string::npos &&
+          arrayName.find("__PRETTY_FUNCTION__") == std::string::npos) {
         std::string varName = gepInst.getName().str();
+        // Remove dots from variable names
+        varName.erase(std::remove(varName.begin(), varName.end(), '.'),
+                      varName.end());
         llvm::Value *arrayIndexOperand =
             gepInst.getOperand(gepInst.getNumOperands() - 1);
         std::string index;
@@ -2821,7 +2887,11 @@
           index = std::to_string(constInt->getSExtValue());
         } else {
           // Variable-index
-          index = "v" + arrayIndexOperand->getName().str();
+          std::string indexName = arrayIndexOperand->getName().str();
+          // Remove dots from index variable names
+          indexName.erase(std::remove(indexName.begin(), indexName.end(), '.'),
+                          indexName.end());
+          index = "v" + indexName;
         }
         llvm::Type *elementType =
             llvm::dyn_cast<llvm::ArrayType>(ptrType)->getElementType();
@@ -2841,6 +2911,12 @@
         std::string varName = gepInst.getName().str();
         std::string structInstance =
             gepInst.getPointerOperand()->getName().str();
+        // Remove dots from variable names to avoid syntax errors in T2 format
+        varName.erase(std::remove(varName.begin(), varName.end(), '.'),
+                      varName.end());
+        structInstance.erase(
+            std::remove(structInstance.begin(), structInstance.end(), '.'),
+            structInstance.end());
         llvm::Value *accessedField =
             gepInst.getOperand(gepInst.getNumOperands() - 1);
         int accessedFieldId =
@@ -2852,11 +2928,17 @@
       } else {
         //<second>
         std::string basePoint = gepInst.getPointerOperand()->getName().str();
+        // Remove dots
+        basePoint.erase(std::remove(basePoint.begin(), basePoint.end(), '.'),
+                        basePoint.end());
         if (std::find(oneDimArrs.begin(), oneDimArrs.end(), "v" + basePoint) !=
             oneDimArrs.end()) {
           // Access an element of an array -> One-dimensional -> Flexible access
           // -> 1 (struct)
           std::string varName = gepInst.getName().str();
+          // Remove dots
+          varName.erase(std::remove(varName.begin(), varName.end(), '.'),
+                        varName.end());
           llvm::Value *arrayIndexOperand =
               gepInst.getOperand(gepInst.getNumOperands() - 1);
           std::string index;
@@ -2864,7 +2946,12 @@
                   llvm::dyn_cast<llvm::ConstantInt>(arrayIndexOperand)) {
             index = std::to_string(constInt->getSExtValue());
           } else {
-            index = "v" + arrayIndexOperand->getName().str();
+            std::string indexVarName = arrayIndexOperand->getName().str();
+            // Remove dots
+            indexVarName.erase(
+                std::remove(indexVarName.begin(), indexVarName.end(), '.'),
+                indexVarName.end());
+            index = "v" + indexVarName;
           }
           std::cout << "v" << varName << " := select_array(v" << basePoint
                     << ", " << index << ");" << std::endl;
@@ -3479,13 +3566,15 @@
       }
       std::cout << "TO: " << basicBlockName << "_" << result << ";\n\n";
       std::cout << "FROM: " << basicBlockName << "_" << result << ";\n";
-      std::cout << "assume(" << leftOperandStr << " " << trueOp << " "
-                << rightOperandStr << ");\n";
+      std::string trueCond =
+          leftOperandStr + " " + trueOp + " " + rightOperandStr;
+      std::cout << "assume(" << ensureValidAssumeCondition(trueCond) << ");\n";
       std::cout << result << " := 1;\n";
       std::cout << "TO: " << basicBlockName << "_s" << result << ";\n\n";
       std::cout << "FROM: " << basicBlockName << "_" << result << ";\n";
-      std::cout << "assume(" << leftOperandStr << " " << falseOp << " "
-                << rightOperandStr << ");\n";
+      std::string falseCond =
+          leftOperandStr + " " + falseOp + " " + rightOperandStr;
+      std::cout << "assume(" << ensureValidAssumeCondition(falseCond) << ");\n";
       std::cout << result << " := 0;\n";
       std::cout << "TO: " << basicBlockName << "_s" << result << ";\n\n";
       std::cout << "FROM: " << basicBlockName << "_s" << result << ";\n";
@@ -3496,11 +3585,14 @@
 
       std::cout << "TO: " << basicBlockName << "_" << result << ";\n\n";
       std::cout << "FROM: " << basicBlockName << "_" << result << ";\n";
-      std::cout << "assume(" << (c->toT2String()) << ");\n";
+      std::cout << "assume(" << ensureValidAssumeCondition(c->toT2String())
+                << ");\n";
       std::cout << result << " := 1;\n";
       std::cout << "TO: " << basicBlockName << "_s" << result << ";\n\n";
       std::cout << "FROM: " << basicBlockName << "_" << result << ";\n";
-      std::cout << "assume(" << ((c->toNNF(true))->toT2String()) << ");\n";
+      std::cout << "assume("
+                << ensureValidAssumeCondition((c->toNNF(true))->toT2String())
+                << ");\n";
       std::cout << result << " := 0;\n";
       std::cout << "TO: " << basicBlockName << "_s" << result << ";\n\n";
       std::cout << "FROM: " << basicBlockName << "_s" << result << ";\n";
@@ -3577,6 +3669,11 @@
     //<Negar>
     std::string leftVar = I.getName().str();
     std::string rightVar = I.getOperand(0)->getName().str();
+    // Remove dots from variable names to avoid syntax errors in T2 format
+    leftVar.erase(std::remove(leftVar.begin(), leftVar.end(), '.'),
+                  leftVar.end());
+    rightVar.erase(std::remove(rightVar.begin(), rightVar.end(), '.'),
+                   rightVar.end());
     if (signednessInfo) {
       unsigned destinationBits = I.getType()->getIntegerBitWidth();
       std::cout << "v" << leftVar << " := extract(" << (destinationBits - 1)

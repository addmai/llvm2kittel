--- /tmp/tmp.QMgVR8dB4J/old_file.cpp	2026-03-27 17:06:18.149742703 +0900
+++ /tmp/tmp.QMgVR8dB4J/new_file.cpp	2026-03-27 17:06:18.221739678 +0900
@@ -307,7 +307,9 @@
         } else {
           // Define an array -> Globally -> One-dimensional -> 1
           std::string arrayName = "v" + varName;
-          if (GV.hasInitializer() &&
+          // Check if this is an integer array and zero-initialized
+          bool isIntegerArray = varType->getArrayElementType()->isIntegerTy();
+          if (isIntegerArray && GV.hasInitializer() &&
               llvm::isa<llvm::ConstantAggregateZero>(
                   const_cast<llvm::Constant *>(GV.getInitializer()))) {
             globalInsts.push_back(arrayName + " := const_array(0);");
@@ -1154,7 +1156,8 @@
       break;
     }
     llvm::PHINode *phi = llvm::cast<llvm::PHINode>(i);
-    if (phi->getType() == m_boolType || !phi->getType()->isIntegerTy()) {
+    // Skip non-integer types (but process bool as i1)
+    if (!phi->getType()->isIntegerTy()) {
       continue;
     }
     std::string PHIName = getVar(phi);
@@ -1277,6 +1280,17 @@
         // will print transition from true to BB
         llvm::BasicBlock *iBlock = branch->getSuccessor(0);
         if (m_t2Output && !m_currentBlockReachErrorCalled) {
+          // Output phi assignments for iBlock
+          for (llvm::BasicBlock::iterator pi = iBlock->begin();
+               llvm::isa<llvm::PHINode>(pi); ++pi) {
+            llvm::PHINode *phi = llvm::cast<llvm::PHINode>(pi);
+            if (phi->getType()->isIntegerTy()) {
+              llvm::Value *val = phi->getIncomingValueForBlock(pBlock);
+              std::cout << "var__temp_" << getVar(phi)
+                        << " := " << getPolynomial(val)->toString() << ";"
+                        << std::endl;
+            }
+          }
           std::cout << "TO: " << (iBlock->getName().str()) << ";" << std::endl;
         }
       } else {
@@ -1354,6 +1368,17 @@
                         << ensureValidAssumeCondition(c->toT2String()) << ");"
                         << std::endl;
               llvm::BasicBlock *tBlock = branch->getSuccessor(0);
+              // Output phi assignments for tBlock
+              for (llvm::BasicBlock::iterator pi = tBlock->begin();
+                   llvm::isa<llvm::PHINode>(pi); ++pi) {
+                llvm::PHINode *phi = llvm::cast<llvm::PHINode>(pi);
+                if (phi->getType()->isIntegerTy()) {
+                  llvm::Value *val = phi->getIncomingValueForBlock(pBlock);
+                  std::cout << "var__temp_" << getVar(phi)
+                            << " := " << getPolynomial(val)->toString() << ";"
+                            << std::endl;
+                }
+              }
               std::cout << "TO: " << (tBlock->getName().str()) << ";"
                         << std::endl
                         << std::endl;
@@ -1366,6 +1391,17 @@
                                (c->toNNF(true))->toT2String())
                         << ");" << std::endl;
               llvm::BasicBlock *fBlock = branch->getSuccessor(1);
+              // Output phi assignments for fBlock
+              for (llvm::BasicBlock::iterator pi = fBlock->begin();
+                   llvm::isa<llvm::PHINode>(pi); ++pi) {
+                llvm::PHINode *phi = llvm::cast<llvm::PHINode>(pi);
+                if (phi->getType()->isIntegerTy()) {
+                  llvm::Value *val = phi->getIncomingValueForBlock(pBlock);
+                  std::cout << "var__temp_" << getVar(phi)
+                            << " := " << getPolynomial(val)->toString() << ";"
+                            << std::endl;
+                }
+              }
               std::cout << "TO: " << (fBlock->getName().str()) << ";"
                         << std::endl;
             }
@@ -1377,6 +1413,17 @@
                       << ensureValidAssumeCondition(c->toT2String()) << ");"
                       << std::endl;
             llvm::BasicBlock *tBlock = branch->getSuccessor(0);
+            // Output phi assignments for tBlock
+            for (llvm::BasicBlock::iterator pi = tBlock->begin();
+                 llvm::isa<llvm::PHINode>(pi); ++pi) {
+              llvm::PHINode *phi = llvm::cast<llvm::PHINode>(pi);
+              if (phi->getType()->isIntegerTy()) {
+                llvm::Value *val = phi->getIncomingValueForBlock(pBlock);
+                std::cout << "var__temp_" << getVar(phi)
+                          << " := " << getPolynomial(val)->toString() << ";"
+                          << std::endl;
+              }
+            }
             std::cout << "TO: " << (tBlock->getName().str()) << ";" << std::endl
                       << std::endl;
 
@@ -1388,6 +1435,17 @@
                              (c->toNNF(true))->toT2String())
                       << ");" << std::endl;
             llvm::BasicBlock *fBlock = branch->getSuccessor(1);
+            // Output phi assignments for fBlock
+            for (llvm::BasicBlock::iterator pi = fBlock->begin();
+                 llvm::isa<llvm::PHINode>(pi); ++pi) {
+              llvm::PHINode *phi = llvm::cast<llvm::PHINode>(pi);
+              if (phi->getType()->isIntegerTy()) {
+                llvm::Value *val = phi->getIncomingValueForBlock(pBlock);
+                std::cout << "var__temp_" << getVar(phi)
+                          << " := " << getPolynomial(val)->toString() << ";"
+                          << std::endl;
+              }
+            }
             std::cout << "TO: " << (fBlock->getName().str()) << ";"
                       << std::endl;
           }
@@ -2663,8 +2721,8 @@
           // Special handling for error functions (reach_error, abort,
           // __VERIFIER_error, etc.)
           std::string funcName = callee->getName().str();
-          if (funcName == "reach_error" || funcName == "abort" ||
-              funcName == "__VERIFIER_error" || funcName == "__assert_fail") {
+          if (funcName == "reach_error" || funcName == "__VERIFIER_error" ||
+              funcName == "__assert_fail") {
             if (m_ignoreReachError) {
               // For termination analysis: treat reach_error() as empty function
               // Don't generate ERROR state or transitions
@@ -2680,6 +2738,10 @@
               m_reachErrorCalled = true;
             }
             // Continue with normal non-returning behavior
+          } else if (funcName == "abort") {
+            // abort() is NOT treated as reach_error() for unreach-call
+            // verification It's a program termination, not an error state Let
+            // it be handled as unreachable (normal non-returning function)
           }
           // they don't mess with globals
           continue;

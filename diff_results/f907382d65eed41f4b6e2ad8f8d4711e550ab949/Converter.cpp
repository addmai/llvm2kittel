--- /tmp/tmp.QMgVR8dB4J/old_file.cpp	2026-03-27 17:06:18.306736108 +0900
+++ /tmp/tmp.QMgVR8dB4J/new_file.cpp	2026-03-27 17:06:18.383732874 +0900
@@ -1349,6 +1349,17 @@
                         << std::endl;
               std::cout << "assume(" << phiVar << " == true);" << std::endl;
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
@@ -1358,6 +1369,17 @@
                         << std::endl;
               std::cout << "assume(" << phiVar << " == false);" << std::endl;
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
             } else {

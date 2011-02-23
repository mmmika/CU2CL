#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/Expr.h"
#include "clang/AST/ExprCXX.h"
#include "clang/AST/Stmt.h"

#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/SourceManager.h"

#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendPluginRegistry.h"

#include "clang/Lex/Preprocessor.h"

#include "clang/Rewrite/Rewriter.h"

#include "llvm/Support/raw_ostream.h"

#include <list>
#include <set>
#include <sstream>
#include <string>

using namespace clang;

namespace {

/**
 * An AST consumer made to rewrite CUDA to OpenCL.
 **/
class RewriteCUDA : public ASTConsumer {
private:
    CompilerInstance *CI;
    SourceManager *SM;
    Preprocessor *PP;
    Rewriter Rewrite;

    //Rewritten files
    FileID MainFileID;
    //std::set<FileID> IncludedFileIDs;

    llvm::raw_ostream *MainOutFile;
    llvm::raw_ostream *KernelOutFile;

    std::set<llvm::StringRef> Kernels;
    std::set<VarDecl *> DeviceMems;

    FunctionDecl *MainDecl;

    std::string Preamble;
    //TODO break Preamble up into different portions that are combined

    std::string CLInit;
    std::string CLClean;

    void TraverseStmt(Stmt *e, unsigned int indent) {
        for (unsigned int i = 0; i < indent; i++)
            llvm::errs() << "  ";
        llvm::errs() << e->getStmtClassName() << "\n";
        indent++;
        for (Stmt::child_iterator CI = e->child_begin(), CE = e->child_end();
             CI != CE; ++CI)
            TraverseStmt(*CI, indent);
    }

    void FindLeafStmts(Stmt *s, std::vector<Stmt *> &leaves) {
        if (s->children().empty()) {
            leaves.push_back(s);
        }
        else {
            for (Stmt::child_iterator CI = s->child_begin(), CE = s->child_end();
                 CI != CE; ++CI) {
                FindLeafStmts(*CI, leaves);
            }
        }
    }

    DeclRefExpr *FindDeclRefExpr(Stmt *e) {
        if (DeclRefExpr *dr = dyn_cast<DeclRefExpr>(e))
            return dr;
        DeclRefExpr *ret = NULL;
        for (Stmt::child_iterator CI = e->child_begin(), CE = e->child_end();
             CI != CE; ++CI) {
            ret = FindDeclRefExpr(*CI);
            if (ret)
                return ret;
        }
        return NULL;

    }

    void RewriteHostStmt(Stmt *s) {
        //TODO recurse
        /*for (Stmt::child_iterator CI = s->child_begin(), CE = s->child_end();
             CI != CE; ++CI) {
            RewriteHostStmt(*CI);
        }*/
        //TODO visit
        //TODO for recursively going through Stmts
        //TODO support for, while, do-while, if, CompoundStmts
        /*switch (body->getStmtClass()) {
        }
        */
    }

    void RewriteCUDAKernel(FunctionDecl *cudaKernel) {
        llvm::errs() << "Rewriting CUDA kernel\n";
        Kernels.insert(cudaKernel->getName());
        Preamble += "cl_kernel clKernel_" + cudaKernel->getName().str() + ";\n";
        //TODO find way to rewrite attributes
        //GlobalAttr *ga = fd->getAttr<GlobalAttr>();
        //Rewrite.ReplaceText(ga->getLocation(), sizeof(char)*(sizeof("__global__")-1), "__kernel");
        //Rewrite.ReplaceText(cudaKernel->getLocStart(), Rewrite.getRangeSize(cudaKernel->getSourceRange()), "__kernel");
        //TODO rewrite arguments
        for (FunctionDecl::param_iterator PI = cudaKernel->param_begin(), PE = cudaKernel->param_end();
             PI != PE; ++PI) {
        }
        if (cudaKernel->hasBody()) {
            RewriteKernelStmt(cudaKernel->getBody());
            //TraverseStmt(cudaKernel->getBody(), 0);
        }
    }

    void RewriteKernelStmt(Stmt *ks) {
        //TODO recurse
        for (Stmt::child_iterator CI = ks->child_begin(), CE = ks->child_end();
             CI != CE; ++CI) {
            RewriteKernelStmt(*CI);
        }
        //TODO visit
        std::string SStr;
        llvm::raw_string_ostream S(SStr);
        switch (ks->getStmtClass()) {
            case Stmt::MemberExprClass:
                //llvm::errs() << ((MemberExpr *) ks)->getMemberNameInfo().getAsString() << "\n";
                ks->printPretty(S, 0, PrintingPolicy(Rewrite.getLangOpts()));
                if (S.str() == "threadIdx.x")
                    ReplaceStmtWithText(ks, "get_local_id(0)");
                else if (S.str() == "threadIdx.y")
                    ReplaceStmtWithText(ks, "get_local_id(1)");
                else if (S.str() == "threadIdx.z")
                    ReplaceStmtWithText(ks, "get_local_id(2)");
                else if (S.str() == "blockIdx.x")
                    ReplaceStmtWithText(ks, "get_group_id(0)");
                else if (S.str() == "blockIdx.y")
                    ReplaceStmtWithText(ks, "get_group_id(1)");
                else if (S.str() == "blockIdx.z")
                    ReplaceStmtWithText(ks, "get_group_id(2)");
                else if (S.str() == "blockDim.x")
                    ReplaceStmtWithText(ks, "get_local_size(0)");
                else if (S.str() == "blockDim.y")
                    ReplaceStmtWithText(ks, "get_local_size(1)");
                else if (S.str() == "blockDim.z")
                    ReplaceStmtWithText(ks, "get_local_size(2)");
                else if (S.str() == "gridDim.x")
                    ReplaceStmtWithText(ks, "get_group_size(0)");
                else if (S.str() == "gridDim.y")
                    ReplaceStmtWithText(ks, "get_group_size(1)");
                else if (S.str() == "gridDim.z")
                    ReplaceStmtWithText(ks, "get_group_size(2)");
                break;
            case Stmt::DeclRefExprClass:
                //llvm::errs() << ((DeclRefExpr *) ks)->getNameInfo().getAsString() << "\n";
                //ks->printPretty(S, 0, PrintingPolicy(Rewrite.getLangOpts()));
                //llvm::errs() << S.str() << "\n";
                break;
            case Stmt::CallExprClass:
                ks->printPretty(S, 0, PrintingPolicy(Rewrite.getLangOpts()));
                if (S.str() == "__syncthreads()")
                    ReplaceStmtWithText(ks, "barrier(CLK_LOCAL_MEM_FENCE)");
            default:
                break;
        }
    }

    void RewriteCUDACall(CallExpr *cudaCall) {
        llvm::errs() << "Rewriting CUDA API call\n";
        std::string funcName = cudaCall->getDirectCallee()->getNameAsString();
        if (funcName == "cudaThreadExit") {
            //Replace with clReleaseContext()
            ReplaceStmtWithText(cudaCall, "clReleaseContext(clContext)");
        }
        else if (funcName == "cudaThreadSynchronize") {
            //Replace with clFinish
            ReplaceStmtWithText(cudaCall, "clFinish(clCommandQueue)");
        }
        else if (funcName == "cudaSetDevice") {
            //TODO implement
            llvm::errs() << "cudaSetDevice not implemented yet\n";
        }
        else if (funcName == "cudaMalloc") {
            //TODO check if the return value is being placed somewhere
            //TODO case if cudaMalloc being used as argument to something
            Expr *varExpr = cudaCall->getArg(0);
            Expr *size = cudaCall->getArg(1);
            DeclRefExpr *dr = FindDeclRefExpr(varExpr);
            VarDecl *var = dyn_cast<VarDecl>(dr->getDecl());
            llvm::StringRef varName = var->getName();
            std::string SStr;
            llvm::raw_string_ostream S(SStr);
            size->printPretty(S, 0, PrintingPolicy(Rewrite.getLangOpts()));

            //Replace with clCreateBuffer
            std::string sub = varName.str() + " = clCreateBuffer(clContext, CL_MEM_READ_WRITE, " + S.str() + ", NULL, NULL)";
            ReplaceStmtWithText(cudaCall, sub);

            //Change variable's type to cl_mem
            TypeLoc tl = var->getTypeSourceInfo()->getTypeLoc();
            Rewrite.ReplaceText(tl.getBeginLoc(),
                                Rewrite.getRangeSize(tl.getSourceRange()),
                                "cl_mem ");

            //Add var to DeviceMems
            DeviceMems.insert(var);
        }
        else if (funcName == "cudaFree") {
            Expr *devPtr = cudaCall->getArg(0);
            DeclRefExpr *dr = FindDeclRefExpr(devPtr);
            llvm::StringRef varName = dr->getDecl()->getName();

            //Replace with clReleaseMemObject
            ReplaceStmtWithText(cudaCall, "clReleaseMemObject(" + varName.str() + ")");
        }
        else if (funcName == "cudaMemcpy") {
            std::string replace;
            std::string SStr;
            llvm::raw_string_ostream S(SStr);

            Expr *dst = cudaCall->getArg(0);
            Expr *src = cudaCall->getArg(1);
            Expr *count = cudaCall->getArg(2);
            Expr *kind = cudaCall->getArg(3);
            DeclRefExpr *dr = FindDeclRefExpr(kind);
            EnumConstantDecl *enumConst = dyn_cast<EnumConstantDecl>(dr->getDecl());
            std::string enumString = enumConst->getNameAsString();
            //llvm::errs() << enumString << "\n";
            if (enumString == "cudaMemcpyHostToHost") {
                //standard memcpy
                //TODO make sure to include <string.h>
                replace += "memcpy(";
                dst->printPretty(S, 0, PrintingPolicy(Rewrite.getLangOpts()));
                replace += S.str() + ", ";
                SStr = "";
                src->printPretty(S, 0, PrintingPolicy(Rewrite.getLangOpts()));
                replace += S.str() + ", ";
                SStr = "";
                count->printPretty(S, 0, PrintingPolicy(Rewrite.getLangOpts()));
                replace += S.str() + ")";
                ReplaceStmtWithText(cudaCall, replace);
            }
            else if (enumString == "cudaMemcpyHostToDevice") {
                //clEnqueueWriteBuffer
                replace += "clEnqueueWriteBuffer(clCommandQueue, ";
                dst->printPretty(S, 0, PrintingPolicy(Rewrite.getLangOpts()));
                replace += S.str() + ", CL_TRUE, 0, ";
                SStr = "";
                count->printPretty(S, 0, PrintingPolicy(Rewrite.getLangOpts()));
                replace += S.str() + ", ";
                SStr = "";
                src->printPretty(S, 0, PrintingPolicy(Rewrite.getLangOpts()));
                replace += S.str() + ", 0, NULL, NULL)";
                ReplaceStmtWithText(cudaCall, replace);
            }
            else if (enumString == "cudaMemcpyDeviceToHost") {
                //clEnqueueReadBuffer
                replace += "clEnqueueReadBuffer(clCommandQueue, ";
                src->printPretty(S, 0, PrintingPolicy(Rewrite.getLangOpts()));
                replace += S.str() + ", CL_TRUE, 0, ";
                SStr = "";
                count->printPretty(S, 0, PrintingPolicy(Rewrite.getLangOpts()));
                replace += S.str() + ", ";
                SStr = "";
                dst->printPretty(S, 0, PrintingPolicy(Rewrite.getLangOpts()));
                replace += S.str() + ", 0, NULL, NULL)";
                ReplaceStmtWithText(cudaCall, replace);
            }
            else if (enumString == "cudaMemcpyDeviceToDevice") {
                //TODO clEnqueueReadBuffer -> clEnqueueWriteBuffer
                ReplaceStmtWithText(cudaCall, "clEnqueueReadBuffer(clCommandQueue, src, CL_TRUE, 0, count, temp, 0, NULL, NULL)");
                ReplaceStmtWithText(cudaCall, "clEnqueueWriteBuffer(clCommandQueue, dst, CL_TRUE, 0, count, temp, 0, NULL, NULL)");
            }
            else {
                //TODO Use diagnostics to print pretty errors
                llvm::errs() << "Unsupported cudaMemcpy type: " << enumString << "\n";
            }
        }
        else if (funcName == "cudaMemset") {
            //TODO follow Swan's example of setting via a kernel
            //TODO add memset kernel to the list of kernels
            /*Expr *devPtr = cudaCall->getArg(0);
            Expr *value = cudaCall->getArg(1);
            Expr *count = cudaCall->getArg(2);*/
        }
        else {
            //TODO Use diagnostics to print pretty errors
            llvm::errs() << "Unsupported CUDA call: " << funcName << "\n";
        }
    }

    void RewriteCUDAKernelCall(CUDAKernelCallExpr *kernelCall) {
        llvm::errs() << "Rewriting CUDA kernel call\n";
        FunctionDecl *callee = kernelCall->getDirectCallee();
        CallExpr *kernelConfig = kernelCall->getConfig();
        std::string kernelName = "clKernel_" + callee->getNameAsString();
        std::ostringstream args;
        for (unsigned i = 0; i < kernelCall->getNumArgs(); i++) {
            Expr *arg = kernelCall->getArg(i);
            VarDecl *var = dyn_cast<VarDecl>(FindDeclRefExpr(arg)->getDecl());
            args << "clSetKernelArg(" << kernelName << ", " << i << ", sizeof(";
            if (DeviceMems.find(var) != DeviceMems.end()) {
                //arg var is a cl_mem
                args << "cl_mem";
            }
            else {
                args << var->getType().getAsString();
            }
            args << "), &" << var->getNameAsString() << ");\n";
        }
        //TODO guaranteed to be dim3s, so pull out their x,y,z values
        //TODO if constants, create global size_t arrays for them
        Expr *grid = kernelConfig->getArg(0);
        Expr *block = kernelConfig->getArg(1);
        //TraverseStmt(grid, 0);
        //TraverseStmt(block, 0);
        //TODO check if these are constants or variables
        if (CXXConstructExpr *construct = dyn_cast<CXXConstructExpr>(grid)) {
            //constant passed
            std::vector<Stmt *> gridExprs;
            std::vector<Stmt *> blockExprs;
            std::ostringstream globalWorkSize[3];
            std::ostringstream localWorkSize[3];
            FindLeafStmts(block, blockExprs);
            for (unsigned int i = 0; i < 3; i++) {
                localWorkSize[i] << "localWorkSize[" << i << "] = ";
                if (IntegerLiteral *intArg = dyn_cast<IntegerLiteral>(blockExprs[i])) {
                    localWorkSize[i] << intArg->getValue().toString(10, true);
                }
                else if (CXXDefaultArgExpr *defArg = dyn_cast<CXXDefaultArgExpr>(blockExprs[i])) {
                    localWorkSize[i] << "1";
                }
                else {
                    //TODO unimplemented argument
                }
                localWorkSize[i] << ";\n";
                args << localWorkSize[i].str();
            }
            FindLeafStmts(grid, gridExprs);
            for (unsigned int i = 0; i < 3; i++) {
                globalWorkSize[i] << "globalWorkSize[" << i << "] = ";
                if (IntegerLiteral *intArg = dyn_cast<IntegerLiteral>(gridExprs[i])) {
                    globalWorkSize[i] << intArg->getValue().toString(10, true);
                }
                else if (CXXDefaultArgExpr *defArg = dyn_cast<CXXDefaultArgExpr>(gridExprs[i])) {
                    globalWorkSize[i] << "1";
                }
                else {
                    //TODO unimplemented argument
                }
                globalWorkSize[i] << "*localWorkSize[" << i << "];\n";
                args << globalWorkSize[i].str();
            }
        }
        else {
            //TODO variable passed
        }
        args << "clEnqueueNDRangeKernel(clCommandQueue, " << kernelName << ", 3, NULL, globalWorkSize, localWorkSize, 0, NULL, NULL)";
        ReplaceStmtWithText(kernelCall, args.str());
    }

    void RewriteMain(FunctionDecl *mainDecl) {
        MainDecl = mainDecl;
    }

    bool ReplaceStmtWithText(Stmt *OldStmt, llvm::StringRef NewStr) {
        return Rewrite.ReplaceText(OldStmt->getLocStart(),
                                   Rewrite.getRangeSize(OldStmt->getSourceRange()),
                                   NewStr);
    }

public:
    RewriteCUDA(llvm::raw_ostream *HostOS, llvm::raw_ostream *KernelOS, CompilerInstance *comp) :
        ASTConsumer(), MainOutFile(HostOS), KernelOutFile(KernelOS), CI(comp) { }

    virtual ~RewriteCUDA() { }

    virtual void Initialize(ASTContext &Context) {
        SM = &Context.getSourceManager();
        PP = &CI->getPreprocessor();
        Rewrite.setSourceMgr(Context.getSourceManager(), Context.getLangOptions());
        MainFileID = Context.getSourceManager().getMainFileID();

        Preamble += "#ifdef __APPLE__\n";
        Preamble += "#include <OpenCL/opencl.h>\n";
        Preamble += "#else\n";
        Preamble += "#include <CL/opencl.h>\n";
        Preamble += "#endif\n\n";
        Preamble += "cl_platform_id clPlatform;\n";
        Preamble += "cl_device_id clDevice;\n";
        Preamble += "cl_context clContext;\n";
        Preamble += "cl_command_queue clCommandQueue;\n";
        Preamble += "cl_program clProgram;\n\n";
        Preamble += "size_t globalWorkSize[3];\n";
        Preamble += "size_t localWorkSize[3];\n\n";
    }

    virtual void HandleTopLevelDecl(DeclGroupRef DG) {
        //TODO check where the declaration comes from (may have been included)
        for(DeclGroupRef::iterator i = DG.begin(), e = DG.end(); i != e; ++i) {
            SourceLocation loc = (*i)->getLocation();
            //PresumedLoc pl = SM->getPresumedLoc(d->getLocation());
            //llvm::errs() << pl.getFilename() << "\n";
            if (SM->isFromMainFile(loc) /*||
                strstr(SM->getPresumedLoc(loc).getFilename(), ".cu") != NULL*/) {
                if (FunctionDecl *fd = dyn_cast<FunctionDecl>(*i)) {
                    if (fd->hasAttr<CUDAGlobalAttr>() || fd->hasAttr<CUDADeviceAttr>()) {
                        //Is a device function
                        RewriteCUDAKernel(fd);
                    }
                    else if (Stmt *body = fd->getBody()) {
                        assert(body->getStmtClass() == Stmt::CompoundStmtClass &&
                               "Invalid statement: Not a statement class");
                        CompoundStmt *cs = dyn_cast<CompoundStmt>(body);
                        //llvm::errs() << "Number of Stmts: " << cs->size() << "\n";
                        for (Stmt::child_iterator ci = cs->child_begin(), ce = cs->child_end();
                             ci != ce; ++ci) {
                            if (Stmt *childStmt = *ci) {
                                //llvm::errs() << "Child Stmt: " << childStmt->getStmtClassName() << "\n";
                                if (CallExpr *ce = dyn_cast<CallExpr>(childStmt)) {
                                    llvm::errs() << "\tCallExpr: ";
                                    if (CUDAKernelCallExpr *kce = dyn_cast<CUDAKernelCallExpr>(ce)) {
                                        llvm::errs() << kce->getDirectCallee()->getName() << "\n";
                                        RewriteCUDAKernelCall(kce);
                                    }
                                    else if (FunctionDecl *callee = ce->getDirectCallee()) {
                                        std::string calleeName = callee->getNameAsString();
                                        //llvm::errs() << calleeName << "\n";
                                        if (calleeName.find("cuda") == 0) {
                                            RewriteCUDACall(ce);
                                        }
                                    }
                                    else
                                        llvm::errs() << "??\n";
                                }
                            }
                        }
                    }
                    if (fd->getNameAsString() == "main") {
                        RewriteMain(fd);
                    }
                }
                else if (VarDecl *vd = dyn_cast<VarDecl>(*i)) {
                    //TODO get type of the variable, if dim3 rewrite to size_t[3]
                    //TODO check other CUDA-only types to rewrite
                }
            }
        }
    }

    virtual void HandleTranslationUnit(ASTContext &) {
        //Add global CL declarations
        Rewrite.InsertTextBefore(SM->getLocForStartOfFile(MainFileID), Preamble);

        CompoundStmt *mainBody = dyn_cast<CompoundStmt>(MainDecl->getBody());
        //Add opencl initialization stuff at top of main
        CLInit += "\n";
        CLInit += "clGetPlatformIDs(1, &clPlatform, NULL);\n";
        CLInit += "clGetDeviceIDs(clPlatform, CL_DEVICE_TYPE_GPU, 1, &clDevice, NULL);\n";
        CLInit += "clContext = clCreateContext(NULL, 1, &clDevice, NULL, NULL, NULL);\n";
        CLInit += "clCommandQueue = clCreateCommandQueue(clContext, clDevice, 0, NULL);\n";
        CLInit += "clProgram = clCreateProgramWithSource(clContext, count, strings, lengths, NULL);\n";
        CLInit += "clBuildProgram(clProgram, 1, &clDevice, NULL, NULL, NULL);\n";
        for (std::set<llvm::StringRef>::iterator it = Kernels.begin();
             it != Kernels.end(); it++) {
            std::string kernelName = (*it).str();
            CLInit += "clKernel_" + kernelName + " = clCreateKernel(clProgram, \"" + kernelName + "\", NULL);\n";
        }
        Rewrite.InsertTextAfter(PP->getLocForEndOfToken(mainBody->getLBracLoc()), CLInit);

        //Add cleanup code at bottom of main
        CLClean += "\n";
        for (std::set<llvm::StringRef>::iterator it = Kernels.begin();
             it != Kernels.end(); it++) {
            std::string kernelName = (*it).str();
            CLClean += "clReleaseKernel(clKernel_" + kernelName + ");\n";
        }
        CLClean += "clReleaseProgram(clProgram);\n";
        CLClean += "clReleaseCommandQueue(clCommandQueue);\n";
        CLClean += "clReleaseContext(clContext);\n";
        Rewrite.InsertTextBefore(mainBody->getRBracLoc(), CLClean);

        //Write the rewritten buffer to a file
        if (const RewriteBuffer *RewriteBuff =
            Rewrite.getRewriteBufferFor(MainFileID)) {
            *MainOutFile << std::string(RewriteBuff->begin(), RewriteBuff->end());
        }
        else {
            //TODO use diagnostics for pretty errors
            llvm::errs() << "No changes made!\n";
        }
        //TODO Write the rewritten kernel buffers to new files

        MainOutFile->flush();
    }

};

class RewriteCUDAAction : public PluginASTAction {
protected:
    ASTConsumer *CreateASTConsumer(CompilerInstance &CI, llvm::StringRef InFile) {
        if (llvm::raw_ostream *HostOS =
            CI.createDefaultOutputFile(false, InFile, "c")) {
            /*if (llvm::raw_ostream *KernelOS =
                CI.createDefaultOutputFile(false, InFile, "cl")) {
                return new RewriteCUDA(HostOS, KernelOS);
            }*/
            return new RewriteCUDA(HostOS, NULL, &CI);
        }
        return NULL;
    }

    bool ParseArgs(const CompilerInstance &CI,
                   const std::vector<std::string> &args) {
        for (unsigned i = 0, e = args.size(); i != e; ++i) {
            llvm::errs() << "RewriteCUDA arg = " << args[i] << "\n";
            //TODO parse arguments
        }
        if (args.size() && args[0] == "help")
            PrintHelp(llvm::errs());

        return true;
    }

    void PrintHelp(llvm::raw_ostream &ros) {
        ros << "Help for RewriteCUDA plugin goes here\n";
    }

};

}

static FrontendPluginRegistry::Add<RewriteCUDAAction>
X("rewrite-cuda", "translate CUDA kernels to OpenCL");
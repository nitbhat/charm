#include "CStateVar.h"
#include "SdagEntry.h"
#include "xi-Chare.h"
#include "xi-Entry.h"
#include "xi-Parameter.h"
#include <list>

namespace xi {

SdagEntryConstruct::SdagEntryConstruct(SdagConstruct* body)
    : SdagConstruct(SSDAGENTRY, body) {}

SdagEntryConstruct::SdagEntryConstruct(SListConstruct* body)
    : SdagConstruct(SSDAGENTRY, body) {}

void SdagEntryConstruct::generateCode(XStr& decls, XStr& defs, XStr &sdagFuncName, bool isDummy) {
  XStr signature;
  signature << sdagFuncName;
  signature << "(";
  if (stateVars) {
    int count = 0;
    for (std::list<CStateVar*>::iterator iter = stateVars->begin();
         iter != stateVars->end(); ++iter) {
      CStateVar& var = **iter;
      if (var.isVoid != 1) {
        if (count != 0) signature << ", ";
        if (var.byConst) signature << "const ";
        if (var.type != 0) signature << var.type << " ";
        if (var.arrayLength != NULL) signature << "* ";
        if (var.declaredRef) signature << "& ";
        if (var.name != 0) signature << var.name;
        count++;
      }
    }
  }
  signature << ")";

  if (isDummy) {
    decls << "  void " << signature << ";\n";
    // CkAbort is added inside the dummy function definition to throw a runtime error when
    // an SDAG entry method containing a when clause is called directly, without a proxy
    generateLocalWrapper(decls, defs, entry->param->isVoid(), signature, entry, stateVars,
                         con1->text, isDummy);
    return;
  }

  if (!entry->param->isVoid()) {
    decls << "  void " << signature << ";\n";
  }

  // generate wrapper for local calls to the function
  if (entry->paramIsMarshalled() && !entry->param->isVoid())
    generateLocalWrapper(decls, defs, entry->param->isVoid(), signature, entry, stateVars,
                         con1->text);

  generateClosureSignature(decls, defs, entry, false, "void", (const XStr *)&sdagFuncName, false,
                           encapState);
}

void SdagEntryConstruct::generateCode(XStr& decls, XStr& defs, Entry* entry) {
  if (entry->isConstructor()) {
    std::cerr << cur_file << ":" << entry->getLine()
              << ": Chare constructor cannot be defined with SDAG code" << std::endl;
    exit(1);
  }

  buildTypes(encapState);
  buildTypes(encapStateChild);

  decls << "public:\n";

  XStr sdagFuncName;
  if(entry->containsWhenConstruct) {
    // generate a dummy method for throwing a compilation error using static assert
    sdagFuncName << con1->text;
    generateCode(decls, defs, sdagFuncName, true);
    sdagFuncName.clear();

    // generate the sdag entry method code with a function name prefixed with "_sdag_fnc_"
    sdagFuncName <<  "_sdag_fnc_" << con1->text;
    generateCode(decls, defs, sdagFuncName);
  } else {

    // generate code normally for sdag entry methods that do not contain a when construct
    sdagFuncName << con1->text;
    generateCode(decls, defs, sdagFuncName);
  }

#if CMK_BIGSIM_CHARM
  generateEndSeq(defs);
#endif
  if (!entry->getContainer()->isGroup() || !entry->isConstructor())
    generateTraceEndCall(defs, 1);

  defs << "  if (!__dep.get()) _sdag_init();\n";

  // is a message sdag entry, in this case since this is a SDAG entry, there
  // will only be one parameter which is the message (called 'gen0')
  if (!entry->paramIsMarshalled() && !entry->param->isVoid()) {
    // increase reference count by one for the state parameter
    defs << "  CmiReference(UsrToEnv(gen0));\n";
  }

  defs << "  ";
  generateCall(defs, encapStateChild, encapStateChild, constructs->front()->label);

#if CMK_BIGSIM_CHARM
  generateTlineEndCall(defs);
  generateBeginExec(defs, "spaceholder");
#endif
  if (!entry->getContainer()->isGroup() || !entry->isConstructor())
    generateDummyBeginExecute(defs, 1, entry);

  endMethod(defs);

  decls << "private:\n";
  generateClosureSignature(decls, defs, entry, false, "void", con1->text, true,
#if CMK_BIGSIM_CHARM
                           encapStateChild
#else
                           encapState
#endif
                           );

  if (!entry->paramIsMarshalled() && !entry->param->isVoid()) {
    // decrease reference count by one for the message
    defs << "  CmiFree(UsrToEnv(gen0));\n";
  }

  endMethod(defs);

  generateChildrenCode(decls, defs, entry);
}

void SdagEntryConstruct::numberNodes() {
  nodeNum = numSdagEntries++;
  SdagConstruct::numberNodes();
}

void SdagEntryConstruct::labelNodes() {
  label = createLabel(con1->text->charstar(), -1);
  SdagConstruct::labelNodes();
}

}  // namespace xi

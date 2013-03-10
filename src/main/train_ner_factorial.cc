#include "base.h"

#include "crf.h"
#include "crf/main.h"
#include "main.h"

int run(int argc, char *argv[]) {
  return NLP::CRF::run_train_factorial<NLP::CRF::NERFactorial>(argc, argv, "", "%p %c %e", "lbfgs");
}

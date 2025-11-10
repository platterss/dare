#ifndef AUTHENTICATION_H
#define AUTHENTICATION_H

#include "task/Task.h"

// Authenticates user and saves registration time. Doesn't do anything if already authenticated.
void authenticate(Task& task);

#endif // AUTHENTICATION_H
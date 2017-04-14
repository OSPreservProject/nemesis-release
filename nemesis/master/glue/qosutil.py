#
# nemmaster buildutils
#
# Copyright 1998 Dickon Reed
#
# QoS utilities
#

import types, string
from nemclasses import period, slice, latency, extra
InvalidTime = 'InvalidTime'

def timeasns(x):
    if type(x) == types.StringType:
	s = string.lower(x)
	if s[-1] != 's':
	    raise InvalidTime, x
	modifier = 1000*1000*1000
	sproper = s[:-1]
	if s[-2] == 'm': 
	    modifier = 1000*1000
	    sproper = s[:-2]
	elif s[-2] == 'u':
	    modifier = 1000
	    sproper = s[:-2]
	elif s[-2] == 'n':
	    modifier = 1
	    sproper = s[:-2]
	n = eval(sproper) 
	if type(n) == types.IntType:
	    n = n * modifier
	else:
	    raise InvalidTime, x
	return n
    else:
	if type(x) != types.IntType:
	    raise InvalidTime, x
	return x

def timeasstr(x):
    if type(x) == types.StringType: return x
    if type(x) != types.IntType: raise InvalidTime, x
    if x < 1000: return `x` + 'ns'
    if x < 1000*1000: return `x/1000` + 'us'
    if x < 1000*1000*1000: return `x/1000000` + 'ms'
    return `x/(1000*1000*1000)` + 's'

    
def cpufraction(fraction, periodin = "1ms", extraflag = 1, latencyin = -1):
    latencyout = latencyin
    if latencyout == -1: latencyout = periodin
    periodout = timeasns(periodin)
    sliceout = int(periodout * fraction)

    return {period : timeasstr(periodout), slice:timeasstr(sliceout), latency: timeasstr(latencyout),  extra: extraflag}

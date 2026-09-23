Operator bug: clamp_upper must return hi when v > hi, but uses < . Fix the comparison so max is applied. The program must exit 0 (clamp_upper(20,10) == 10).

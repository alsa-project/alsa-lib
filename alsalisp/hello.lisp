(princ "Hello ALSA world\n")
(princ "One " 1 "\n")
(princ "Two " (+ 1 1) "\n")

(defun myprinc (o) (princ o))
(myprinc "Printed via myprinc function!\n")

(defun printnum (from to) (while (<= from to) (princ " " from) (setq from (+ from 1))))
(princ "Numbers 1-10: ") (printnum 1 10) (princ "\n")

(defun factorial (n) (when (> n 0) (* n (factorial (- n 1)))))
(princ "Factorial of 10: " (factorial 10) "\n")

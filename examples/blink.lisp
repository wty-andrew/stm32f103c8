(defvar led 17)
(pinmode led t)

(defun blink ()
  (loop
    (digitalwrite led t)
    (delay 1000)
    (digitalwrite led nil)
    (delay 1000)))

(blink)

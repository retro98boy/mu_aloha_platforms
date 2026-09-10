.text
.global _start
_start:
    /* AD6C has already validated x20->i2c_cfg. Keep its failure path;
     * do not open I2C or touch the redriver, PMIC or GPI.
     */
    b stock_failure

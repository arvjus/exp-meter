#!/usr/bin/python3

import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
from sklearn import linear_model, metrics
from statistics import mean

def mkFilterFn(c):
    print(f"""
// {c[0]:.9f}  {c[1]:.9f}  {c[2]:.9f}
float calculate_filter_fb(float ev)
{{
    float filter = ({c[0]:.9f}f * ev + {c[1]:.9f}f) * ev + {c[2]:.9f}f;

    filter = roundf(filter * 2.0f) * 0.5f;

    if (filter < 0.0f) return 0.0f;
    if (filter > 5.0f) return 5.0f;

    return filter;
}}
""")

# read dataset, split features/targets
data = pd.read_csv("calibration_fb.csv", index_col=False)
data['contrast_ev'] = np.log(data['exp_l']/data['exp_h'])/np.log(2)
print(data)

# fit model
coef = np.polyfit(data["contrast_ev"], data["filter"], 2)

pred = np.polyval(coef, data["contrast_ev"])
pred_round = np.round(pred * 2) / 2

print(pd.DataFrame({
    "actual": data["filter"],
    "pred": pred,
    "rounded": pred_round,
    "error": pred_round - data["filter"]
}))

# test extrapolation
ev = np.linspace(1.2, 4.5, 200)   # extend below your lowest measured EV
filter = np.polyval(coef, ev)

# plot graph
plt.figure(figsize=(8,6))
plt.scatter(data["contrast_ev"], data["filter"], color="red", label="Measured")
plt.plot(ev, filter, label="Quadratic")
plt.grid()
plt.legend()
plt.show()

mkFilterFn(coef)

#!/usr/bin/python3

import numpy as np
import pandas as pd
from sklearn import linear_model, metrics
import matplotlib.pyplot as plt
from statistics import mean

# read dataset, split features/targets
data = pd.read_csv("calibration_fb.csv", index_col=False)
data['contrast_ev'] = np.log(data['exp_l']/data['exp_h'])/np.log(2)-1.68
print(data)


def scatter():
    plt.figure("Scatter", figsize=(12, 5))

    plt.title("Filter")
    plt.grid()
    plt.scatter(data.iloc[:,3], data.iloc[:,0], label='contr', color='g')
    plt.legend(loc='upper right')

    m = (((mean(data.iloc[:,3]) * mean(data.iloc[:,0])) - mean(data.iloc[:,3] * data.iloc[:,0])) /
         ((mean(data.iloc[:,3]) * mean(data.iloc[:,3])) - mean(data.iloc[:,3] * data.iloc[:,3])))
    b = mean(data.iloc[:,0]) - m * mean(data.iloc[:,3])
    plt.plot(data.iloc[:,3], [(m*x)+b for x in data.iloc[:,3]])
    plt.show()

def runLinearRegression(X, y):
    regr = linear_model.LinearRegression()
    model = regr.fit(X, y)
    return (model.intercept_,) + tuple(model.coef_), model.predict(X)

def runRidge(X, y, alpha):
    regr = linear_model.Ridge(alpha=alpha)
    model = regr.fit(X, y)
    return (model.intercept_,) + tuple(model.coef_), model.predict(X)

def mkFilter2Fn(coef):
    print("""
    // %f   %f
    float calculate_filter(float ev_contrast)
    {
      float filter = round((%f + ev_contrast * %f) * 2) / 2.0f;
      return filter < 0 ? 0.0f : filter > 5 ? 5.0f : filter;
    }
    """ % (coef + coef))



#scatter()


lr_coef, lr_pred = runLinearRegression(data.iloc[:,3:4], data.iloc[:,0])
#lr_coef, lr_pred = runRidge(data.iloc[:,3:4], data.iloc[:,0], 0.1)
print("rmse:  ", np.sqrt(metrics.mean_squared_error(data.iloc[:,0], lr_pred)))
print("score: ", metrics.r2_score(data.iloc[:,0], lr_pred))
print(data.iloc[:,0] - lr_pred)
print(lr_coef)
print()

mkFilter2Fn(lr_coef)

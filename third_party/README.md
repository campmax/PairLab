# Third-party notices

- `json.hpp`: nlohmann/json 3.11.3, MIT license retained in the header and
  JSON_LICENSE.txt. Source: https://github.com/nlohmann/json/releases/tag/v3.11.3
- MacKinnon 2010 critical-value coefficients in statistics.hpp are derived from
  statsmodels' two-series, constant-specification table. Its BSD license is in
  statsmodels-LICENSE.txt. Source: https://github.com/statsmodels/statsmodels

There are no linked external C++ libraries. Charts use browser-native SVG with
no CDN or frontend dependencies. Independent validation optionally uses Python
NumPy/statsmodels and Node jsdom; those packages are not bundled or needed to run
the engine or view reports.

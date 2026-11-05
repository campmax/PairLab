For the newly added live-data dashboard, use **START_LIVE.md**.
The instructions below concern the original historical backtest.

# Start here — Windows

## 1. Open the finished result

Extract the entire ZIP first. Do not run files from inside the compressed folder.
Double-click **OPEN_REPORT.cmd**. It opens **docs/index.html** in your browser.

The report has five tabs:

1. **Performance:** results, portfolio chart, drawdowns, benchmarks and stresses.
2. **Pair research:** choose a pair, view its price/spread charts, and inspect every
   accepted or rejected training window.
3. **Trade ledger:** see both the signal date and execution date for each trade.
4. **Diagnostics:** rejected filters, exit reasons, trade outcomes and cost attribution.
5. **How it works:** plain-language methodology, exact settings and limitations.

Everything is local. Reading the report does not need a server, an account, a
market-data API key, or Python. It does not update automatically; it represents
one frozen historical experiment.

## 2. Compile the C++ program

You already have Visual Studio for the previous projects. Double-click
**build_windows.cmd**. It locates Visual Studio's C++ compiler, builds the engine,
builds the tests, and runs them. Wait for **BUILD AND TESTS PASSED**.

If the compiler cannot be found, open Visual Studio Installer and install the
**Desktop development with C++** workload, then try again.

The build script stops on failure. Do not run an old executable after a failed
build. Windows compilation is included for you to verify; development validation
was performed with GCC on Linux.

## 3. Reproduce the experiment

Double-click **RUN_RESEARCH.cmd**. It runs the baseline and all four stress
scenarios using the included real dataset and opens the new report. Each run
gets a new folder under `runs`; previous reports remain unchanged.

You do not need to install Python packages or download data to reproduce the
included result.

## 4. Read and change one assumption

Open `config/default.json` in Notepad or Visual Studio. Save a copy before
changing it. For example, change `borrow_rate` from `0.03` to `0.10` to model
more expensive short borrowing. Run the experiment again and compare the reports.

Do not repeatedly adjust rules to maximize the historical return and then present
that same period as independent validation. Record every experiment in a research
journal, including unsuccessful changes.

## Command-line usage

Open PowerShell in the extracted PairLab folder:

```powershell
.\bin\pairlab.exe --data data\real --config config\default.json --out runs\my_experiment --stress
```

The output folder must not already exist. Omit `--stress` for the baseline only.

To download a new copy of the same 2010–2023 dataset:

```powershell
py tools\fetch_data.py --out data\fresh
```

The downloader uses Python's standard library. It may fail if the public provider
changes its interface or rate-limits requests; the archived dataset still works.
It refuses to overwrite an existing data folder.

## Later holdout study

The default end date is exclusive: 2024-01-01. After fixing the rules, obtain a new
dataset that includes the reserved period, make a new configuration with
`start` set to 2024-01-01 and a later `end_exclusive`, and run with
`--allow-holdout`. Keep at least 504 earlier sessions in that dataset for warmup.
The gate is a reminder, not a claim that you have never seen the period before.

## Other platforms

```sh
g++ -std=c++17 -O2 src/main.cpp -o pairlab
g++ -std=c++17 -O2 tests/tests.cpp -o pairlab_tests
./pairlab_tests
./pairlab --out runs/example --stress
```

Or build with CMake. Run the program with the project root as your working
directory so it can find the config, data and report template.

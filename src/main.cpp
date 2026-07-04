#include <bits/stdc++.h>
using namespace std;

// ---------- 快速 IO ----------
inline void fast_io() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    cout << fixed << setprecision(2);
}

// ---------- 全局参数 ----------
int N;              // 股票数 200
int D;              // 交易日总数
int L;              // 1手股数 100
double M;           // 初始现金
double ALPHA;       // 佣金费率
double COM_MIN;     // 最低佣金
double BETA;        // 印花税率
int K_MAX;          // 单日最大委托数

// ---------- 股票相关 ----------
vector<string> CODES;                     // 代码
unordered_map<string, int> CODE2IDX;      // 代码 -> 索引
vector<vector<double>> CLOSES;            // CLOSES[day][idx]
vector<vector<double>> OPENS;             // OPENS[day][idx]

// ---------- 账户状态 ----------
double cash = 0.0;
vector<int> holdings;                     // 当前持股数

// ---------- 工具函数 ----------
inline double round2(double x) {
    return round(x * 100.0) / 100.0;
}

// 分割字符串（逗号或空格）
vector<string> split(const string& s) {
    vector<string> res;
    string cur;
    for (char c : s) {
        if (c == ',' || c == ' ') {
            if (!cur.empty()) {
                res.push_back(cur);
                cur.clear();
            }
        } else {
            cur += c;
        }
    }
    if (!cur.empty()) res.push_back(cur);
    return res;
}

// 解析一行行情，返回结构化对象
struct StockDay {
    string code;
    double open, high, low, close;
    long long volume;
    double amount, amplitude, pct_chg, change, turnover;
};
StockDay parse_line(const string& line) {
    auto tok = split(line);
    StockDay sd;
    sd.code = tok[1];
    sd.open = stod(tok[2]);
    sd.high = stod(tok[3]);
    sd.low = stod(tok[4]);
    sd.close = stod(tok[5]);
    sd.volume = stoll(tok[6]);
    sd.amount = stod(tok[7]);
    sd.amplitude = stod(tok[8]);
    sd.pct_chg = stod(tok[9]);
    sd.change = stod(tok[10]);
    sd.turnover = stod(tok[11]);
    return sd;
}

// ---------- 本地撮合模拟 ----------
// 严格按题目规则更新 cash 和 holdings
void simulate_execution(const vector<string>& orders,
                        const vector<double>& exec_prices,
                        const vector<double>& prev_close) {
    for (const string& order : orders) {
        istringstream iss(order);
        string cmd, code;
        int qty;
        iss >> cmd >> code >> qty;
        auto it = CODE2IDX.find(code);
        if (it == CODE2IDX.end()) continue;
        int idx = it->second;
        double price = exec_prices[idx];
        if (price <= 0.0) continue;                // 停牌

        if (cmd == "BUY") {
            // 涨停判断：price >= 上一日收盘价 * 1.10 则拒绝
            double prev = prev_close[idx];
            if (prev > 0.0) {
                double limit_up = round2(prev * 1.10);
                if (price >= limit_up - 1e-9) continue;
            }
            double V = price * qty;
            double comm = max(COM_MIN, round2(ALPHA * V));
            double cost = V + comm;
            if (cash >= cost - 1e-9) {
                cash = round2(cash - cost);
                holdings[idx] += qty;
            }
        } else if (cmd == "SELL") {
            if (holdings[idx] >= qty) {
                double V = price * qty;
                double tax = round2(BETA * V);
                double comm = max(COM_MIN, round2(ALPHA * V));
                double income = V - tax - comm;
                cash = round2(cash + income);
                holdings[idx] -= qty;
            }
        }
    }
}

// ---------- 策略：简单动量（示例） ----------
vector<string> generate_orders(int day) {
    vector<string> orders;
    if (day < 5) return orders;              // 至少需要5天历史

    const int LOOKBACK = 5;
    const int TOP_N = 5;

    // 计算动量
    vector<double> momentum(N, -1e9);
    for (int i = 0; i < N; ++i) {
        double start = CLOSES[day - LOOKBACK][i];
        double end = CLOSES[day][i];
        if (start > 0.0)
            momentum[i] = (end - start) / start;
    }

    // 选出 TOP_N 动量最大的股票（仅正动量）
    vector<int> idxs(N);
    iota(idxs.begin(), idxs.end(), 0);
    sort(idxs.begin(), idxs.end(), [&](int a, int b) {
        return momentum[a] > momentum[b];
    });

    vector<int> targets;
    for (int i = 0; i < N && targets.size() < TOP_N; ++i) {
        if (momentum[idxs[i]] > 0.0)
            targets.push_back(idxs[i]);
    }
    unordered_set<int> target_set(targets.begin(), targets.end());

    // 估算当前总资产（用于计算目标市值）
    double total_equity = cash;
    for (int i = 0; i < N; ++i) {
        if (holdings[i] > 0)
            total_equity += holdings[i] * CLOSES[day][i];
    }

    double estimated_cash = cash;

    // 第一步：清仓非目标股票
    for (int i = 0; i < N; ++i) {
        if (holdings[i] > 0 && target_set.find(i) == target_set.end()) {
            int qty = holdings[i];
            orders.push_back("SELL " + CODES[i] + " " + to_string(qty));
            double V = CLOSES[day][i] * qty;
            double tax = round2(BETA * V);
            double comm = max(COM_MIN, round2(ALPHA * V));
            estimated_cash = round2(estimated_cash + V - tax - comm);
        }
    }

    // 第二步：调整目标持仓至等权重
    if (!targets.empty()) {
        double target_val = total_equity / targets.size();

        // 对目标持仓，先卖后买
        for (int i : targets) {
            double price = CLOSES[day][i];
            if (price <= 0.0) continue;
            double cur_val = holdings[i] * price;
            if (cur_val > target_val) {
                double delta = cur_val - target_val;
                int sell_qty = (int)(delta / price) / L * L;
                sell_qty = min(sell_qty, holdings[i]);
                if (sell_qty > 0) {
                    orders.push_back("SELL " + CODES[i] + " " + to_string(sell_qty));
                    double V = price * sell_qty;
                    double tax = round2(BETA * V);
                    double comm = max(COM_MIN, round2(ALPHA * V));
                    estimated_cash = round2(estimated_cash + V - tax - comm);
                }
            }
        }

        // 买入（按动量排序，动量高的优先占用资金）
        sort(targets.begin(), targets.end(), [&](int a, int b) {
            return momentum[a] > momentum[b];
        });
        for (int i : targets) {
            double price = CLOSES[day][i];
            if (price <= 0.0) continue;
            double cur_val = holdings[i] * price;
            if (cur_val < target_val) {
                double delta = target_val - cur_val;
                int want_qty = (int)(delta / price) / L * L;
                if (want_qty <= 0) continue;

                // 预估每手成本（含佣金），控制买单规模
                double approx_cost_per_lot = price * L * (1.0 + ALPHA) + COM_MIN;
                int max_lots = (int)(estimated_cash / approx_cost_per_lot);
                if (max_lots <= 0) continue;

                int buy_qty = min(want_qty, max_lots * L);
                buy_qty = buy_qty / L * L;
                if (buy_qty <= 0) continue;

                orders.push_back("BUY " + CODES[i] + " " + to_string(buy_qty));
                double V = price * buy_qty;
                double comm = max(COM_MIN, round2(ALPHA * V));
                estimated_cash = round2(estimated_cash - (V + comm));
            }
        }
    }

    // 遵守 K_MAX 限制（如果超过则截断，实际策略应尽量不超过）
    if ((int)orders.size() > K_MAX) {
        orders.resize(K_MAX);
    }
    return orders;
}

// ---------- 主交互 ----------
int main() {
    fast_io();

    string line;
    getline(cin, line);
    auto init_tok = split(line);
    // INIT N D M L ALPHA COM_MIN BETA K_MAX
    N = stoi(init_tok[1]);
    D = stoi(init_tok[2]);
    M = stod(init_tok[3]);
    L = stoi(init_tok[4]);
    ALPHA = stod(init_tok[5]);
    COM_MIN = stod(init_tok[6]);
    BETA = stod(init_tok[7]);
    K_MAX = stoi(init_tok[8]);

    CODES.resize(N);
    CLOSES.assign(D + 1, vector<double>(N, 0.0));
    OPENS.assign(D + 1, vector<double>(N, 0.0));

    // 读取 day 0 行情
    for (int i = 0; i < N; ++i) {
        getline(cin, line);
        auto sd = parse_line(line);
        CODES[i] = sd.code;
        CODE2IDX[sd.code] = i;
        OPENS[0][i] = sd.open;
        CLOSES[0][i] = sd.close;
    }

    cash = M;
    holdings.assign(N, 0);
    int day = 0;

    while (true) {
        vector<string> orders;
        if (day == D) {
            // 最后一日，在收盘价执行
            orders = generate_orders(day);
            for (const string& o : orders) cout << o << '\n';
            cout << "DONE" << endl;  // endl 会 flush
            simulate_execution(orders, CLOSES[day],
                               (day > 0 ? CLOSES[day-1] : vector<double>(N, 0.0)));
            getline(cin, line);      // 等待 FINISH
            break;
        } else {
            orders = generate_orders(day);
            for (const string& o : orders) cout << o << '\n';
            cout << "DONE" << endl;
            // 读取 day+1 行情
            for (int i = 0; i < N; ++i) {
                getline(cin, line);
                auto sd = parse_line(line);
                int idx = CODE2IDX[sd.code];
                OPENS[day+1][idx] = sd.open;
                CLOSES[day+1][idx] = sd.close;
            }
            simulate_execution(orders, OPENS[day+1], CLOSES[day]);
            ++day;
        }
    }

    return 0;
}
#include "search.h"


Search::Search()
{
    //set defaults here
}

Search::~Search() {}


SearchResult Search::startSearch(ILogger* Logger, const Map& map, const EnvironmentOptions& options)
{
    std::chrono::time_point<std::chrono::system_clock> start, finish;
    start = std::chrono::system_clock::now();

    Node s;
    s.i = map.getStartI();
    s.j = map.getStartJ();
    s.g = 0;
    s.parent = nullptr;
    s.H = getHeuristic(s.i, s.j, map, options);
    s.F = s.g + s.H;
    bool pathFound(false);

    // It is made to avoid same hashing for (i, j), (j, i) nodes
    OPEN.insert({ s.j + s.i * map.getMapWidth(), s });
    while (!OPEN.empty()) {
        s = argmin(options);
        OPEN.erase(s.j + s.i * map.getMapWidth());
        CLOSED.insert({ s.j + s.i * map.getMapWidth(), s });
        //Check if current node is our "finish" node
        if (s.i == map.getGoalI() && s.j == map.getGoalJ()) {
            //We found the path to the finish node
            pathFound = true;
            break;
        }

        std::list<Node> successors = getSuccessors(s, map, options);
        //Here are some cases to not choose the node
        //1) Node not in OPEN
        //2) Node cost is inappropriate
        for (auto iter = successors.begin(); iter != successors.end(); ++iter) {
            if ((OPEN.find(iter->i * map.getMapWidth() + iter->j) == OPEN.end()) ||
                (iter->F <= OPEN[iter->i * map.getMapWidth() + iter->j].F) &&
                (((options.breakingties) && (iter->g >= OPEN[iter->i * map.getMapWidth() + iter->j].g)) ||
                (!(options.breakingties) && (iter->g <= OPEN[iter->i * map.getMapWidth() + iter->j].g)))) {

                iter->parent = &CLOSED.find(s.i * map.getMapWidth() + s.j)->second;
                *iter = changeParent(*iter, *(iter->parent), map, options);
                OPEN.erase(iter->i * map.getMapWidth() + iter->j);
                OPEN.insert({ iter->i * map.getMapWidth() + iter->j , *iter });
            }
        }

        Logger->writeToLogOpenClose(OPEN, CLOSED, false);

    }
    Logger->writeToLogOpenClose(OPEN, CLOSED, false);

    if (pathFound) {
        sresult.pathlength = s.g;
        makePrimaryPath(s);
    }

    finish = std::chrono::system_clock::now();
    sresult.time = static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(finish - start).count()) / 1000000000;

    if (pathFound) makeSecondaryPath();

    sresult.pathfound = pathFound;
    sresult.nodescreated = CLOSED.size() + OPEN.size();
    sresult.numberofsteps = CLOSED.size();

    sresult.hppath = &hppath;
    sresult.lppath = &lppath;

    return sresult;
}

double Search::getHeuristic(int x1, int y1, int x2, int y2, const EnvironmentOptions& options) {
    double dx(abs(x1 - x2)), dy(abs(y1 - y2));

    if (options.searchtype < 2) { return 0.0; }

    if (options.metrictype == CN_SP_MT_DIAG) { return (std::min(dx, dy) * sqrt(2)) + abs(dx - dy); }

    if (options.metrictype == CN_SP_MT_MANH) { return (dx + dy); }

    if (options.metrictype == CN_SP_MT_EUCL) { return (sqrt((x1 - x2) * (x1 - x2) + (y1 - y2) * (y1 - y2))); }

    if (options.metrictype == CN_SP_MT_CHEB) { return std::max(dx, dy); }
}

double Search::getHeuristic(int x1, int y1, const Map& map, const EnvironmentOptions& options) {
    int x2(map.getGoalI()), y2(map.getGoalJ());
    double dx(abs(x1 - x2)), dy(abs(y1 - y2));

    if (options.searchtype < 2) { return 0.0; }

    if (options.metrictype == CN_SP_MT_DIAG) { return (std::min(dx, dy) * sqrt(2)) + abs(dx - dy); }

    if (options.metrictype == CN_SP_MT_MANH) { return (dx + dy); }

    if (options.metrictype == CN_SP_MT_EUCL) { return (sqrt((x1 - x2) * (x1 - x2) + (y1 - y2) * (y1 - y2))); }

    if (options.metrictype == CN_SP_MT_CHEB) { return std::max(dx, dy); }
}

//Finds min cost node in OPEN
Node Search::argmin(const EnvironmentOptions& options) {
    Node minNode = (OPEN.begin())->second;
    for (auto iter = OPEN.begin(); iter != OPEN.end(); ++iter) {
        Node cur = iter->second;
        if (cur.F < minNode.F) {
            minNode = cur;
        } else if (cur.F == minNode.F) {
            if (cur.g <= minNode.g) {
                minNode = cur;
            }
        }
    }
    return minNode;
}

std::list<Node> Search::getSuccessors(Node s, const Map& map, const EnvironmentOptions& options) {
    std::list<Node> successors;
    Node successor;
    bool noWay;
    for (int down = -1; down < 2; ++down) {
        for (int right = -1; right < 2; ++right) {
            noWay = false;
            if (right && down) {
                if (map.CellOnGrid(s.i + down, s.j + right) &&
                    map.CellIsTraversable(s.i + down, s.j + right)) {
                    if ((down != 0) && (right != 0)) {
                        if ((map.CellIsObstacle(s.i + down, s.j) &&
                            map.CellIsObstacle(s.i, s.j + right) && 
                            !(options.allowsqueeze))) noWay = true;
                    }

                    if (!options.allowdiagonal) noWay = true;

                    if ((map.CellIsObstacle(s.i + down, s.j)) ||
                        (map.CellIsObstacle(s.i, s.j + right)) &&
                        !(options.cutcorners)) noWay = true;
                }
            }
            //If there is a way and node not in CLOSED
            if ((!noWay) && (CLOSED.find((s.i + down) * map.getMapWidth() + (s.j + right)) == CLOSED.end())) {
                successor.i = s.i + down;
                successor.j = s.j + right;
                if ((down != 0) && (right != 0)) successor.g = s.g + sqrt(2);
                else successor.g = s.g + 1;
                successor.H = getHeuristic(successor.i, successor.j, map, options);
                successor.F = successor.g + successor.H * options.hweight;
                successors.push_front(successor);
            }
        }
    }
    return successors;
}

bool Search::lineOfSight(int x1, int y1, int x2, int y2, const Map& map, const EnvironmentOptions& options) {
    int dx, dy, x_inc, y_inc, error, error0, n, x, y;

    dx = abs(x2 - x1);
    dy = abs(y2 - y1);
    x_inc = (x2 > x1 ? 1 : -1);
    y_inc = (y2 > y1 ? 1 : -1);
    n = dx + dy;
    error = dx - dy;
    error0 = error;
    x = x1;
    y = y1;
    dx *= 2;
    dy *= 2;

    if (dx == 0) {
        for (; y != y2; y += y_inc) {
            if (map.CellIsObstacle(x, y))
                return false;
        }
        return true;
    }
    else if (dy == 0) {
        for (; x != x2; x += x_inc) {
            if (map.CellIsObstacle(x, y))
                return false;
        }
        return true;
    }

    while (n > 0) {
        if (map.CellIsObstacle(x, y)) {
            return false;
        }

        if (error > 0) {
            if ((!options.cutcorners) && ((error - dy) < error0)) {
                if (map.CellIsObstacle(x, y + y_inc)) {
                    return false;
                }
            }
            x += x_inc;
            error -= dy;

        }
        else if (error < 0) {
            if ((!options.cutcorners) && ((error + dx) > error0)) {
                if (map.CellIsObstacle(x + x_inc, y)) {
                    return false;
                }
            }
            y += y_inc;
            error += dx;

        }
        else if (error == 0) {
            if (map.CellIsTraversable(x + x_inc, y)) {
                x += x_inc;
                error -= dy;
            }
            else if (map.CellIsTraversable(x, y + y_inc)) {
                y += y_inc;
                error += dx;
            }
            else {
                return false;
            }
        }
        --n;
    }

    if (map.CellIsObstacle(x, y)) {
        return false;
    }

    return true;
}

Node Search::changeParent(Node c, Node p, const Map& map, const EnvironmentOptions& options) {
    if (options.searchtype != CN_SP_ST_TH) return c;

    if ((p.parent == nullptr) || ((c.i == p.parent->i) && (c.j == p.parent->j))) return c;

    if (lineOfSight(p.parent->i, p.parent->j, c.i, c.j, map, options)) {
        c.g = p.parent->g + getHeuristic(c.i, c.j, p.i, p.j, options);
        c.F = c.g + (options.hweight * c.H);
        c.parent = p.parent;
    }
    return c;
}

void Search::makePrimaryPath(Node currentNode) {
    Node thisNode = currentNode;

    while (thisNode.parent) {
        lppath.push_front(thisNode);
        thisNode = *(thisNode.parent);
    }
    lppath.push_front(thisNode);
}

void Search::makeSecondaryPath()
{
    auto it = lppath.begin();
    int currentNode_i, currentNode_j, nextNode_i, nextNode_j;
    hppath.push_back(*it);

    while (it != --lppath.end()) {
        currentNode_i = it->i;
        currentNode_j = it->j;

        ++it;
        nextNode_i = it->i;
        nextNode_j = it->j;

        ++it;

        if (((it->i - nextNode_i) != (nextNode_i - currentNode_i)) || ((it->j - nextNode_j) != (nextNode_j - currentNode_j))) {
            hppath.push_back(*(--it));
        }
        else {
            --it;
        }
    }
}

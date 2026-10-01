// =============================================================================
// ceo_email_queue.cpp
//
// Prioritizes emails for a busy CEO using a hand-built MaxHeap that serves as
// a priority queue.
//
// Design overview
// ---------------
//   Email         - a simple object holding one email's data plus the logic
//                   that decides whether it outranks another email.
//   MaxHeap       - a from-scratch, list-based (array-backed) binary max-heap
//                   of Email objects. No std::vector, std::priority_queue,
//                   or any other pre-existing heap/container module is used.
//   EmailManager  - reads the test file, interprets each command
//                   (EMAIL / NEXT / READ / COUNT) and drives the MaxHeap.
//
// Priority rules (from the assignment)
// ------------------------------------
//   1. Sender category:
//        Boss                                  -> read first
//        Subordinate / Peer / ImportantPerson  -> read next (all equal level)
//        OtherPerson                           -> read last
//   2. Within the same priority level, the NEWEST email is read first.
//   3. (Extra) If two emails are identical in level AND date, the one that
//      arrived first in the file is read first. This keeps output
//      deterministic, because a heap by itself is not a "stable" structure.
//
// Build:   g++ -std=c++11 -Wall -o ceo_email_queue ceo_email_queue.cpp
// Run:     ./ceo_email_queue testfile.txt
// =============================================================================

#include <iostream>   // std::cout, std::cerr
#include <fstream>    // std::ifstream
#include <string>     // std::string
#include <cstdlib>    // std::atoi
#include <utility>

using namespace std;


// =============================================================================
// class Email
// -----------------------------------------------------------------------------
// Represents a single email in the CEO's inbox.
// An Email knows how to rank itself against another Email, which is the only
// thing the MaxHeap needs in order to keep the most important email on top.
// =============================================================================

// Named priority levels (bigger = read sooner). Used by Email::levelFor().
const int PRIORITY_INVALID = 0;   // unrecognized sender category
const int PRIORITY_LOW     = 1;   // OtherPerson
const int PRIORITY_NORMAL  = 2;   // Subordinate, Peer, ImportantPerson
const int PRIORITY_BOSS    = 3;   // Boss

class Email {
private:
    string senderCategory;  // "Boss", "Subordinate", "Peer", ...
    string subject;         // subject line (may contain spaces, never commas)
    string date;            // original text, MM-DD-YYYY (kept for display)
    int    dateKey;         // YYYYMMDD integer so dates compare numerically
    int    arrivalOrder;    // order the email was added; used as a tie-breaker
    int priority;           // computed once at construction, never recomputed

    // -------------------------------------------------------------------------
    // Convert "MM-DD-YYYY" into the integer YYYYMMDD.
    // Example: "01-03-2025" -> 20250103.
    // A larger number always means a later (newer) date, so comparing two
    // integers is enough to compare two dates. This also handles year
    // rollovers correctly (01-03-2025 > 12-31-2024), which a plain string
    // comparison of "MM-DD-YYYY" would get wrong.
    // -------------------------------------------------------------------------
    static int makeDateKey(const string& d) {
        // Positions in "MM-DD-YYYY": MM = [0,1], DD = [3,4], YYYY = [6..9]
        int month = atoi(d.substr(0, 2).c_str());
        int day   = atoi(d.substr(3, 2).c_str());
        int year  = atoi(d.substr(6, 4).c_str());
        return year * 10000 + month * 100 + day;
    }

public:
    // Default constructor: needed because the heap's array is allocated with
    // `new Email[capacity]`, which default-constructs every slot.
    Email() : senderCategory(""), subject(""), date(""), dateKey(0),
              arrivalOrder(0), priority(PRIORITY_INVALID) {}
    // Full constructor used when an EMAIL command is parsed.
    Email(const string& category, const string& subj, const string& dt, int order)
            : senderCategory(category),
            subject(subj),
            date(dt),
            dateKey(makeDateKey(dt)),
            arrivalOrder(order),
            priority(levelFor(category)) {}   // string comparisons happen only once, here


    // --- Simple accessors ----------------------------------------------------
    string getSenderCategory() const { return senderCategory; }
    string getSubject()        const { return subject; }
    string getDate()           const { return date; }

    // -------------------------------------------------------------------------
    // priorityLevel()
    // Translate the sender category into a number; bigger = read sooner.
    // Subordinate, Peer and ImportantPerson are all "read next", so they share
    // the same level (and are therefore ordered by date among themselves).
    // -------------------------------------------------------------------------

    
    // Single source of truth for category -> priority level.
    static int levelFor(const string& category) {
        if (category == "Boss")            return PRIORITY_BOSS;
        if (category == "Subordinate" ||
            category == "Peer"        ||
            category == "ImportantPerson") return PRIORITY_NORMAL;
        if (category == "OtherPerson")     return PRIORITY_LOW;
        return PRIORITY_INVALID;
    }

    int priorityLevel() const { return priority; }

    bool hasHigherPriorityThan(const Email& other) const {
        if (priority != other.priority)       // integer compare, no strings
            return priority > other.priority;
        if (dateKey != other.dateKey)
            return dateKey > other.dateKey;
        return arrivalOrder < other.arrivalOrder;
    }
};


// =============================================================================
// class MaxHeap
// -----------------------------------------------------------------------------
// A list-based (array-backed) binary max-heap of Email objects.
//
// The complete binary tree is stored level by level in a plain array:
//
//        index:    0    1    2    3    4    5   ...
//        tree:   root  L    R   LL   LR   RL   ...
//
// For the element at index i:
//        parent      = (i - 1) / 2
//        left child  = 2 * i + 1
//        right child = 2 * i + 2
//
// Heap property: every element has priority >= both of its children, so the
// highest-priority email is always at index 0.
//
// The array grows automatically (doubling) so there is no fixed inbox limit.
// =============================================================================
class MaxHeap {
private:
    Email* data;       // dynamically allocated array holding the heap
    int    count;      // how many emails are currently stored
    int    capacity;   // how many emails the array can hold before growing
    static const int MIN_CAPACITY = 8;   // never shrink below this

    // --- Index helpers -------------------------------------------------------
    static int parentOf(int i) { return (i - 1) / 2; }
    static int leftOf(int i)   { return 2 * i + 1; }
    static int rightOf(int i)  { return 2 * i + 2; }

    // Swap two Email objects in the array.
    void swapEmails(int a, int b) {
        Email temp = data[a];
        data[a]    = data[b];
        data[b]    = temp;
    }

    // -------------------------------------------------------------------------
    // grow()
    // Double the array size when it is full: allocate a bigger array, copy
    // everything over, and free the old one.
    // -------------------------------------------------------------------------
    // Reallocate the array to exactly newCapacity slots. Used for both
    // growing and shrinking, so the copy logic lives in one place.
    void resize(int newCapacity) {
        Email* fresh = new Email[newCapacity];
        for (int i = 0; i < count; i++)
            fresh[i] = std::move(data[i]);   // move, don't copy, the strings
        delete[] data;
        data     = fresh;
        capacity = newCapacity;
    }

    void grow() { resize(capacity * 2); }

    // -------------------------------------------------------------------------
    // siftUp()
    // Used after an insert. The new email starts at the bottom of the tree;
    // keep swapping it with its parent while it outranks the parent.
    // -------------------------------------------------------------------------
    void siftUp(int i) {
        while (i > 0) {
            int p = parentOf(i);
            if (data[i].hasHigherPriorityThan(data[p])) {
                swapEmails(i, p);   // child outranks parent -> move it up
                i = p;
            } else {
                break;              // heap property restored
            }
        }
    }

    // -------------------------------------------------------------------------
    // siftDown()
    // Used after removing the root. The last email is moved to the root and
    // may be out of place; keep swapping it with its HIGHER-priority child
    // until it is at least as important as both children.
    // -------------------------------------------------------------------------
    void siftDown(int i) {
        while (true) {
            int left    = leftOf(i);
            int right   = rightOf(i);
            int largest = i;   // index of the highest-priority of {i, left, right}

            if (left < count && data[left].hasHigherPriorityThan(data[largest]))
                largest = left;
            if (right < count && data[right].hasHigherPriorityThan(data[largest]))
                largest = right;

            if (largest == i)
                break;              // node is in the right place

            swapEmails(i, largest);
            i = largest;            // continue from the child's position
        }
    }

public:
    // Constructor: start with a small array; it will grow as needed.
    MaxHeap() : data(new Email[MIN_CAPACITY]), count(0), capacity(MIN_CAPACITY) {}

    // Destructor: release the dynamically allocated array.
    ~MaxHeap() { delete[] data; }

    // Copying a heap is not needed here. Disabling it prevents accidental
    // shallow copies that would double-delete the array.
    MaxHeap(const MaxHeap&)            = delete;
    MaxHeap& operator=(const MaxHeap&) = delete;

    // Number of emails currently waiting.
    int  size()    const { return count; }
    bool isEmpty() const { return count == 0; }

    // -------------------------------------------------------------------------
    // insert()  -  O(log n)
    // Place the email in the first free slot (bottom of the tree), then sift
    // it up to its proper position.
    // -------------------------------------------------------------------------
    void insert(const Email& e) {
        if (count == capacity)
            grow();
        data[count] = e;
        siftUp(count);
        count++;
    }

    // -------------------------------------------------------------------------
    // peekMax()  -  O(1)
    // Look at the highest-priority email WITHOUT removing it (for NEXT).
    // Caller must make sure the heap is not empty.
    // -------------------------------------------------------------------------
    const Email& peekMax() const { return data[0]; }

    // -------------------------------------------------------------------------
    // removeMax()  -  O(log n)
    // Remove the highest-priority email (for READ):
    //   1. Move the last email into the root slot.
    //   2. Shrink the heap by one.
    //   3. Sift the new root down to restore the heap property.
    // Caller must make sure the heap is not empty.
    // -------------------------------------------------------------------------
    void removeMax() {
        data[0] = data[count - 1];
        count--;
        if (count > 0)
            siftDown(0);

        // Shrink when only a quarter full. Using 1/4 (not 1/2) for the trigger
        // stops the array from flip-flopping between grow and shrink when the
        // size hovers near a boundary, so resizing stays amortized O(1).
        if (capacity > MIN_CAPACITY && count <= capacity / 4)
            resize(capacity / 2);
    }
};


// =============================================================================
// class EmailManager
// -----------------------------------------------------------------------------
// Owns the MaxHeap and interprets the commands found in the test file:
//
//     EMAIL <sender category>,<subject line>,<date>   add an email
//     NEXT                                            show the top email
//     READ                                            CEO handled the top email
//     COUNT                                           show unread count
// =============================================================================
class EmailManager {
private:
    MaxHeap inbox;        // the priority queue of unread emails
    int     nextArrival;  // running counter used as the arrival tie-breaker

    // Remove leading/trailing spaces, tabs and carriage returns (so files with
    // Windows "\r\n" line endings still parse correctly).
    static string trim(const string& s) {
        const string whitespace = " \t\r\n";
        size_t start = s.find_first_not_of(whitespace);
        if (start == string::npos)
            return "";                       // string was all whitespace
        size_t end = s.find_last_not_of(whitespace);
        return s.substr(start, end - start + 1);
    }

    // -------------------------------------------------------------------------
    // handleEmail()
    // `args` is everything after "EMAIL ", i.e. "<category>,<subject>,<date>".
    // The subject may contain spaces but never commas, so splitting on the
    // first and last comma is safe.
    // -------------------------------------------------------------------------
    void handleEmail(const string& args) {
        size_t firstComma = args.find(',');
        size_t lastComma  = args.rfind(',');

        // Malformed line: need two distinct commas. Skip it with a warning.
        if (firstComma == string::npos || firstComma == lastComma) {
            cerr << "Warning: skipping malformed EMAIL line: " << args << endl;
            return;
        }

        string category = trim(args.substr(0, firstComma));
        string subject  = trim(args.substr(firstComma + 1, lastComma - firstComma - 1));
        string date     = trim(args.substr(lastComma + 1));

        // Build the Email object and push it into the priority queue.
        inbox.insert(Email(category, subject, date, nextArrival++));
    }

    // -------------------------------------------------------------------------
    // handleNext()  - display (but do not remove) the top-priority email.
    // -------------------------------------------------------------------------
    void handleNext() const {
        if (inbox.isEmpty()) {
            cout << "No emails to read." << endl << endl;
            return;
        }
        const Email& e = inbox.peekMax();
        cout << "Next email:" << endl;
        cout << "\tSender: "  << e.getSenderCategory() << endl;
        cout << "\tSubject: " << e.getSubject()        << endl;
        cout << "\tDate: "    << e.getDate()           << endl;
        cout << endl;
    }

    // -------------------------------------------------------------------------
    // handleRead()  - the CEO has dealt with the top email, so remove it.
    // -------------------------------------------------------------------------
    void handleRead() {
        if (inbox.isEmpty()) {
            cerr << "Warning: READ issued but there are no emails." << endl;
            return;
        }
        inbox.removeMax();
    }

    // -------------------------------------------------------------------------
    // handleCount()  - display the number of unread emails.
    // -------------------------------------------------------------------------
    void handleCount() const {
        cout << "There are " << inbox.size() << " emails to read." << endl << endl;
    }

public:
    EmailManager() : nextArrival(0) {}

    // -------------------------------------------------------------------------
    // processFile()
    // Read the test file line by line and dispatch each command.
    // Returns false if the file could not be opened.
    // -------------------------------------------------------------------------
    bool processFile(const string& filename) {
        ifstream in(filename.c_str());
        if (!in) {
            cerr << "Error: could not open file '" << filename << "'" << endl;
            return false;
        }

        string line;
        while (getline(in, line)) {
            line = trim(line);
            if (line.empty())
                continue;                           // ignore blank lines

            if (line.compare(0, 6, "EMAIL ") == 0) {
                handleEmail(line.substr(6));        // everything after "EMAIL "
            } else if (line == "NEXT") {
                handleNext();
            } else if (line == "READ") {
                handleRead();
            } else if (line == "COUNT") {
                handleCount();
            } else {
                cerr << "Warning: unrecognized command: " << line << endl;
            }
        }
        return true;
    }
};


// =============================================================================
// main
// Usage: ./ceo_email_queue <testfile>
// =============================================================================
int main(int argc, char* argv[]) {
    if (argc < 2) {
        cerr << "Usage: " << argv[0] << " <testfile>" << endl;
        return 1;
    }

    EmailManager manager;
    return manager.processFile(argv[1]) ? 0 : 1;
}

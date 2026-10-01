#include <iostream>
#include <fstream>
#include <sstream>
#include <string>

using namespace std;


/*
 * ============================================================
 * Email Class
 * ============================================================
 *
 * This class represents one email in the CEO's inbox.
 *
 * Each email has:
 *   - A sender category
 *   - A subject
 *   - A date
 *   - A numeric priority for the sender category
 *   - A numeric value for the date
 *
 * The numeric values are used by the MaxHeap to determine which
 * email should be at the top of the heap.
 */
class Email
{
private:
    string sender;
    string subject;
    string date;

    // Higher category priority means the email should be read first.
    int senderPriority;

    // The date converted into a number so dates can easily be compared.
    // For example:
    // 01-03-2025 becomes 20250103
    int dateValue;

public:

    /*
     * Constructor
     *
     * Creates an Email object and calculates the values needed
     * by the MaxHeap.
     */
    Email(string senderCategory, string emailSubject, string emailDate)
    {
        sender = senderCategory;
        subject = emailSubject;
        date = emailDate;

        senderPriority = calculateSenderPriority(senderCategory);
        dateValue = convertDate(emailDate);
    }


    /*
     * Converts the sender category into its priority.
     *
     * The assignment specifies:
     *
     * Boss             - read first
     * Subordinate      - read next
     * Peer              - read next
     * ImportantPerson  - read next
     * OtherPerson      - read last
     *
     * Larger numbers represent higher priority because this is
     * a MaxHeap.
     */
    int calculateSenderPriority(string category)
    {
        if (category == "Boss")
        {
            return 5;
        }
        else if (category == "Subordinate")
        {
            return 4;
        }
        else if (category == "Peer")
        {
            return 3;
        }
        else if (category == "ImportantPerson")
        {
            return 2;
        }
        else
        {
            // OtherPerson
            return 1;
        }
    }


    /*
     * Converts a date in MM-DD-YYYY format into YYYYMMDD.
     *
     * Example:
     *
     * 12-20-2024
     *
     * becomes:
     *
     * 20241220
     *
     * This makes it possible to compare two dates using normal
     * integer comparisons.
     */
    int convertDate(string emailDate)
    {
        int month = stoi(emailDate.substr(0, 2));
        int day = stoi(emailDate.substr(3, 2));
        int year = stoi(emailDate.substr(6, 4));

        return year * 10000 + month * 100 + day;
    }


    /*
     * Getter functions.
     *
     * These allow the MaxHeap to access the information it needs
     * without directly accessing the private data members.
     */
    string getSender()
    {
        return sender;
    }

    string getSubject()
    {
        return subject;
    }

    string getDate()
    {
        return date;
    }

    int getSenderPriority()
    {
        return senderPriority;
    }

    int getDateValue()
    {
        return dateValue;
    }
};


/*
 * ============================================================
 * MaxHeap Class
 * ============================================================
 *
 * This is our priority queue.
 *
 * We are NOT using C++'s priority_queue or any pre-existing
 * heap implementation.
 *
 * Instead, we create the MaxHeap completely from scratch.
 *
 * The heap is represented using a dynamically allocated array.
 *
 * For an item at index i:
 *
 *      Parent = (i - 1) / 2
 *
 *      Left child = 2 * i + 1
 *
 *      Right child = 2 * i + 2
 *
 * The largest/highest-priority email is always stored at index 0.
 */
class MaxHeap
{
private:
    Email** heap;

    // Number of emails currently stored.
    int size;

    // Maximum number of emails the current array can hold.
    int capacity;


    /*
     * Determines whether one email has higher priority than another.
     *
     * Priority is determined in this order:
     *
     * 1. Sender category
     * 2. Date
     *
     * This is important because the assignment's example shows:
     *
     * Boss, 01-03-2025
     *
     * should come before:
     *
     * Boss, 12-20-2024
     *
     * and both Boss emails come before a Subordinate email,
     * even if the Subordinate email has a later date.
     */
   bool higherPriority(Email* first, Email* second)
{
    /*
     * Compare the pre-calculated sender priorities first.
     *
     * Because Email stores senderPriority when it is created,
     * we do not have to calculate the category priority again.
     */
    if (first->getSenderPriority() > second->getSenderPriority())
    {
        return true;
    }

    if (first->getSenderPriority() < second->getSenderPriority())
    {
        return false;
    }

    /*
     * If both emails have the same sender priority,
     * compare their dates.
     *
     * The larger dateValue represents the newer date.
     */
    return first->getDateValue() > second->getDateValue();
}


    /*
     * Doubles the size of the underlying list when it becomes full.
     *
     * This allows the MaxHeap to grow as more emails are added.
     */
    void resize()
    {
        int newCapacity = capacity * 2;

        Email** newHeap = new Email*[newCapacity];

        // Copy the existing email pointers into the larger array.
        for (int i = 0; i < size; i++)
        {
            newHeap[i] = heap[i];
        }

        // Delete the old array.
        delete[] heap;

        // Use the new array.
        heap = newHeap;
        capacity = newCapacity;
    }


    /*
     * Restores the MaxHeap property by moving an email upward.
     *
     * This is called after inserting a new email.
     */
    void heapifyUp(int index)
    {
        while (index > 0)
        {
            int parent = (index - 1) / 2;

            // If the current email has higher priority than its
            // parent, swap them.
            if (higherPriority(heap[index], heap[parent]))
            {
                Email* temp = heap[index];
                heap[index] = heap[parent];
                heap[parent] = temp;

                // Continue checking from the parent's position.
                index = parent;
            }
            else
            {
                // The MaxHeap property has been restored.
                break;
            }
        }
    }


    /*
     * Restores the MaxHeap property by moving an email downward.
     *
     * This is called after removing the highest-priority email.
     */
    void heapifyDown(int index)
    {
        while (true)
        {
            int leftChild = 2 * index + 1;
            int rightChild = 2 * index + 2;

            // Assume the current node has the highest priority.
            int highest = index;


            // Check whether the left child has higher priority.
            if (leftChild < size &&
                higherPriority(heap[leftChild], heap[highest]))
            {
                highest = leftChild;
            }


            // Check whether the right child has higher priority.
            if (rightChild < size &&
                higherPriority(heap[rightChild], heap[highest]))
            {
                highest = rightChild;
            }


            // If neither child has higher priority, the heap
            // property is already correct.
            if (highest == index)
            {
                break;
            }


            // Otherwise, swap the current email with the
            // highest-priority child.
            Email* temp = heap[index];
            heap[index] = heap[highest];
            heap[highest] = temp;

            // Continue from the child's new position.
            index = highest;
        }
    }


public:

    /*
     * Constructor
     *
     * Creates the initial list used by the MaxHeap.
     */
    MaxHeap()
    {
        capacity = 10;
        size = 0;

        heap = new Email*[capacity];
    }


    /*
     * Destructor
     *
     * Deletes the dynamically allocated array.
     *
     * The Email objects themselves are deleted when they are
     * removed from the heap.
     */
    ~MaxHeap()
    {
        delete[] heap;
    }


    /*
     * Inserts an email into the MaxHeap.
     *
     * Steps:
     *
     * 1. Resize if the array is full.
     * 2. Put the email at the end of the heap.
     * 3. Move it upward until the MaxHeap property is restored.
     */
    void insert(Email* email)
    {
        if (size == capacity)
        {
            resize();
        }

        heap[size] = email;

        heapifyUp(size);

        size++;
    }


    /*
     * Returns the highest-priority email without removing it.
     *
     * This is what the NEXT command needs.
     */
    Email* peek()
    {
        if (size == 0)
        {
            return nullptr;
        }

        return heap[0];
    }


    /*
     * Removes and returns the highest-priority email.
     *
     * The root of the MaxHeap is always the highest-priority email.
     *
     * Steps:
     *
     * 1. Save the root.
     * 2. Move the last item to the root.
     * 3. Decrease the size.
     * 4. Heapify downward.
     */
    Email* removeMax()
    {
        if (size == 0)
        {
            return nullptr;
        }

        Email* highestPriorityEmail = heap[0];

        // Move the last email to the root.
        heap[0] = heap[size - 1];

        size--;

        // Restore the MaxHeap property if emails remain.
        if (size > 0)
        {
            heapifyDown(0);
        }

        return highestPriorityEmail;
    }


    /*
     * Returns the current number of unread emails.
     */
    int getSize()
    {
        return size;
    }


    /*
     * Returns whether the heap is empty.
     */
    bool isEmpty()
    {
        return size == 0;
    }
};


/*
 * ============================================================
 * CEOInbox Class
 * ============================================================
 *
 * This class handles the commands from the test file.
 *
 * It uses the MaxHeap as the priority queue for the CEO's emails.
 */
class CEOInbox
{
private:
    MaxHeap emailQueue;


public:

    /*
     * Processes an EMAIL command.
     *
     * The input is expected to look like:
     *
     * EMAIL Boss,Important,12-20-2024
     */
    void addEmail(string emailInformation)
    {
        string sender;
        string subject;
        string date;

        /*
         * stringstream allows us to split the information at
         * commas.
         *
         * The subject is allowed to contain spaces, so we do not
         * split based on spaces.
         */
        stringstream input(emailInformation);

        getline(input, sender, ',');
        getline(input, subject, ',');
        getline(input, date, ',');


        // Create a new Email object.
        Email* newEmail = new Email(sender, subject, date);


        // Add the email to our MaxHeap.
        emailQueue.insert(newEmail);
    }


    /*
     * Handles the NEXT command.
     *
     * NEXT displays the highest-priority unread email but does
     * NOT remove it from the heap.
     */
    void nextEmail()
    {
        Email* email = emailQueue.peek();

        if (email == nullptr)
        {
            cout << "There are no emails to read." << endl;
            return;
        }


        cout << "Next email:" << endl;

        cout << "        Sender: " << email->getSender() << endl;
        cout << "        Subject: " << email->getSubject() << endl;
        cout << "        Date: " << email->getDate() << endl;

        cout << endl;
    }


    /*
     * Handles the READ command.
     *
     * READ means that the CEO has read and dealt with the
     * highest-priority email.
     *
     * Therefore, we remove the maximum element from the heap.
     */
    void readEmail()
    {
        Email* email = emailQueue.removeMax();

        if (email != nullptr)
        {
            // The CEO has now dealt with this email, so the
            // Email object is no longer needed.
            delete email;
        }
    }


    /*
     * Handles the COUNT command.
     *
     * The heap's size is exactly the number of unread emails.
     */
    void countEmails()
    {
        cout << "There are "
             << emailQueue.getSize()
             << " emails to read."
             << endl;

        cout << endl;
    }
};


/*
 * ============================================================
 * MAIN
 * ============================================================
 *
 * The program can process commands either:
 *
 * 1. From a file supplied as a command-line argument:
 *
 *      ./program test.txt
 *
 * or
 *
 * 2. From standard input:
 *
 *      ./program < test.txt
 *
 * The second option is useful for many programming assignments.
 */
int main(int argc, char* argv[])
{
    CEOInbox inbox;

    /*
     * This input stream will either refer to a file or to
     * standard input.
     */
    istream* input = &cin;

    ifstream inputFile;


    /*
     * If a filename was provided as a command-line argument,
     * open that file.
     */
    if (argc > 1)
    {
        inputFile.open(argv[1]);

        if (!inputFile)
        {
            cout << "Error: Could not open input file." << endl;
            return 1;
        }

        input = &inputFile;
    }


    string line;


    /*
     * Read the test file one line at a time.
     */
    while (getline(*input, line))
    {
        /*
         * Ignore empty lines.
         */
        if (line.empty())
        {
            continue;
        }


        /*
         * Determine which command is being used.
         */
        if (line.rfind("EMAIL ", 0) == 0)
        {
            /*
             * Remove "EMAIL " from the beginning of the line.
             *
             * Example:
             *
             * EMAIL Boss,Important,12-20-2024
             *
             * becomes:
             *
             * Boss,Important,12-20-2024
             */
            string emailInformation = line.substr(6);

            inbox.addEmail(emailInformation);
        }
        else if (line == "NEXT")
        {
            inbox.nextEmail();
        }
        else if (line == "READ")
        {
            inbox.readEmail();
        }
        else if (line == "COUNT")
        {
            inbox.countEmails();
        }
    }


    return 0;
}
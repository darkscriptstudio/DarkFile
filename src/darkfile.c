#define _GNU_SOURCE
#define _XOPEN_SOURCE 700

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>
#include <stdatomic.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <libgen.h>
#include <ctype.h>
#include <limits.h>

#define TOOL_VERSION "1.1 (Beta)"
#define TOOL_COMPANY "Dark Script Studio"
#define TOOL_AUTHOR  "Mohammad Tanvir"
#define BUILD_DATE   __DATE__

// --- Platform Compatibility ---
#ifdef _WIN32
    #include <direct.h>
    #define mkdir(p, m) _mkdir(p)
    #include <windows.h>
    #define PATH_SEP '\\'
#else
    #include <sys/sendfile.h>
    #define PATH_SEP '/'
#endif

#define QUEUE_SIZE 32768
#define BUFFER_SIZE (256 * 1024)

typedef enum { MODE_DELETE, MODE_COPY, MODE_MOVE, MODE_COUNT, MODE_ORGANIZE } OperationMode;

typedef struct {
    char *src;
    char *dst;
    char *filename;
} Task;

Task task_queue[QUEUE_SIZE];
int q_head = 0, q_tail = 0, q_count = 0;

pthread_mutex_t q_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t q_cond_not_empty = PTHREAD_COND_INITIALIZER;
pthread_cond_t q_cond_not_full = PTHREAD_COND_INITIALIZER;
pthread_mutex_t mkdir_mutex = PTHREAD_MUTEX_INITIALIZER;

atomic_int active_workers = 0;
atomic_bool done_producing = 0;

atomic_ullong total_bytes = 0;
atomic_ulong total_files = 0;
atomic_ullong processed_bytes = 0;
atomic_ulong processed_files = 0;

// Stats for Organizer
unsigned long count_img=0, count_vid=0, count_aud=0, count_doc=0, count_apk=0, count_arc=0, count_oth=0;
unsigned long long size_img=0, size_vid=0, size_aud=0, size_doc=0, size_apk=0, size_arc=0, size_oth=0;

OperationMode CURRENT_MODE;

const char *ext_images[] = {"jpg", "jpeg", "png", "gif", "webp", "bmp", "heic", NULL};
const char *ext_video[]  = {"mp4", "mkv", "avi", "mov", "flv", "wmv", "3gp", NULL};
const char *ext_audio[]  = {"mp3", "wav", "flac", "aac", "m4a", "ogg", NULL};
const char *ext_docs[]   = {"pdf", "doc", "docx", "txt", "xlsx", "pptx", "csv", NULL};
const char *ext_apks[]   = {"apk", "xapk", "apkm", NULL};
const char *ext_arch[]   = {"zip", "rar", "7z", "tar", "gz", NULL};

// --- Helper: Recursive MKDIR (mkdir -p) ---
int mkdirs(const char *path, mode_t mode) {
    char tmp[4096];
    char *p = NULL;
    size_t len;

    snprintf(tmp, sizeof(tmp), "%s", path);
    len = strlen(tmp);
    if(tmp[len - 1] == PATH_SEP) tmp[len - 1] = 0;

    for (p = tmp + 1; *p; p++) {
        if (*p == PATH_SEP) {
            *p = 0;
            #ifdef _WIN32
            _mkdir(tmp);
            #else
            mkdir(tmp, mode);
            #endif
            *p = PATH_SEP;
        }
    }
    #ifdef _WIN32
    return _mkdir(tmp);
    #else
    return mkdir(tmp, mode);
    #endif
}

// --- CPU Detection ---
int get_cpu_cores() {
    int cores = 1;
    #ifdef _WIN32
        SYSTEM_INFO sysinfo;
        GetSystemInfo(&sysinfo);
        cores = sysinfo.dwNumberOfProcessors;
    #elif defined(_SC_NPROCESSORS_ONLN)
        cores = sysconf(_SC_NPROCESSORS_ONLN);
    #endif
    if (cores < 1) cores = 1;
    return cores;
}

// --- Helper: Post-Processing Cleanup ---
void cleanup_dirs(const char *path) {
    DIR *dir = opendir(path);
    if (!dir) return;
    
    struct dirent *entry;
    char full[PATH_MAX];
    
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".")==0 || strcmp(entry->d_name, "..")==0) continue;
        snprintf(full, sizeof(full), "%s/%s", path, entry->d_name);
        
        struct stat st;
        if (lstat(full, &st) == -1) continue;
        
        if (S_ISDIR(st.st_mode)) {
            cleanup_dirs(full); 
            rmdir(full);        
        }
    }
    closedir(dir);
}

int is_subdirectory(const char *src, const char *dst) {
    if (!dst) return 0;
    char real_src[PATH_MAX];
    char real_dst[PATH_MAX];
    if (!realpath(src, real_src) || !realpath(dst, real_dst)) return 0;
    if (strncmp(real_dst, real_src, strlen(real_src)) == 0) return 1;
    return 0;
}

const char* get_category(const char *filename, int count_stats, unsigned long long filesize) {
    const char *dot = strrchr(filename, '.');
    if(!dot || dot == filename) {
        if(count_stats) { count_oth++; size_oth += filesize; }
        return "Others";
    }
    char ext[32];
    strncpy(ext, dot + 1, 31);
    ext[31] = '\0';
    for(int i=0; ext[i]; i++) ext[i] = tolower(ext[i]);

    for(int i=0; ext_images[i]; i++) if(strcmp(ext, ext_images[i])==0) {
        if(count_stats) { count_img++; size_img += filesize; } return "Images";
    }
    for(int i=0; ext_video[i]; i++)  if(strcmp(ext, ext_video[i])==0) {
        if(count_stats) { count_vid++; size_vid += filesize; } return "Videos";
    }
    for(int i=0; ext_audio[i]; i++)  if(strcmp(ext, ext_audio[i])==0) {
        if(count_stats) { count_aud++; size_aud += filesize; } return "Audio";
    }
    for(int i=0; ext_docs[i]; i++)   if(strcmp(ext, ext_docs[i])==0) {
        if(count_stats) { count_doc++; size_doc += filesize; } return "Documents";
    }
    for(int i=0; ext_apks[i]; i++)   if(strcmp(ext, ext_apks[i])==0) {
        if(count_stats) { count_apk++; size_apk += filesize; } return "APKs";
    }
    for(int i=0; ext_arch[i]; i++)   if(strcmp(ext, ext_arch[i])==0) {
        if(count_stats) { count_arc++; size_arc += filesize; } return "Archives";
    }

    if(count_stats) { count_oth++; size_oth += filesize; }
    return "Others";
}

// --- Copy Logic (Sendfile + Fallback) ---
int standard_copy(int sfd, int dfd) {
    char *buf = malloc(BUFFER_SIZE);
    if (!buf) return -1;
    ssize_t n;
    while ((n = read(sfd, buf, BUFFER_SIZE)) > 0) {
        if (write(dfd, buf, n) != n) { free(buf); return -1; }
        atomic_fetch_add(&processed_bytes, n);
    }
    free(buf);
    return (n == 0) ? 0 : -1;
}

int fast_copy(const char *src, const char *dst) {
    int sfd = open(src, O_RDONLY);
    if (sfd < 0) return -1;
    int dfd = open(dst, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (dfd < 0) { close(sfd); return -1; }

    struct stat st;
    fstat(sfd, &st);
    int success = 0;

    #ifdef __linux__
    if (st.st_size > 0) {
        off_t offset = 0;
        ssize_t sent = 0;
        // Try sendfile (Zero-Copy)
        while (offset < st.st_size) {
            sent = sendfile(dfd, sfd, &offset, st.st_size - offset);
            if (sent < 0) {
                // FALLBACK: If sendfile fails (e.g., android permission issue), use read/write
                lseek(sfd, 0, SEEK_SET);
                lseek(dfd, 0, SEEK_SET);
                atomic_fetch_sub(&processed_bytes, offset); // Reset stats for retry
                if (standard_copy(sfd, dfd) == 0) success = 1;
                goto finish;
            }
            if (sent == 0) break;
            atomic_fetch_add(&processed_bytes, sent);
        }
        if (offset >= st.st_size) success = 1;
    } else success = 1; // Empty file
    #else
    // Standard Copy for Non-Linux
    if (standard_copy(sfd, dfd) == 0) success = 1;
    #endif

finish:
    close(sfd); close(dfd);
    return success ? 0 : -1;
}

void *worker_routine(void *arg) {
    atomic_fetch_add(&active_workers, 1);
    while (1) {
        pthread_mutex_lock(&q_mutex);
        while (q_count == 0 && !done_producing) pthread_cond_wait(&q_cond_not_empty, &q_mutex);
        if (q_count == 0 && done_producing) { pthread_mutex_unlock(&q_mutex); break; }

        Task task = task_queue[q_tail];
        q_tail = (q_tail + 1) % QUEUE_SIZE;
        q_count--;
        pthread_cond_signal(&q_cond_not_full);
        pthread_mutex_unlock(&q_mutex);

        if (CURRENT_MODE == MODE_DELETE) {
            if (remove(task.src) == 0) atomic_fetch_add(&processed_files, 1);
        }
        else if (CURRENT_MODE == MODE_ORGANIZE) {
            const char *cat = get_category(task.filename, 0, 0);
            char target_dir[2048];
            char target_path[4096];
            snprintf(target_dir, sizeof(target_dir), "%s/%s", task.dst, cat);
            snprintf(target_path, sizeof(target_path), "%s/%s", target_dir, task.filename);

            pthread_mutex_lock(&mkdir_mutex);
            mkdirs(target_dir, 0755);
            pthread_mutex_unlock(&mkdir_mutex);

            if (rename(task.src, target_path) != 0) {
                if (fast_copy(task.src, target_path) == 0) unlink(task.src);
            }
            atomic_fetch_add(&processed_files, 1);
        }
        else { 
            if (fast_copy(task.src, task.dst) == 0) {
                if (CURRENT_MODE == MODE_MOVE) unlink(task.src);
                atomic_fetch_add(&processed_files, 1);
            }
        }
        free(task.src);
        if (task.dst) free(task.dst);
        if (task.filename) free(task.filename);
    }
    atomic_fetch_sub(&active_workers, 1);
    return NULL;
}

void enqueue(const char *src, const char *dst, const char *fname) {
    pthread_mutex_lock(&q_mutex);
    while (q_count == QUEUE_SIZE) pthread_cond_wait(&q_cond_not_full, &q_mutex);
    task_queue[q_head].src = strdup(src);
    task_queue[q_head].dst = dst ? strdup(dst) : NULL;
    task_queue[q_head].filename = fname ? strdup(fname) : NULL;
    q_head = (q_head + 1) % QUEUE_SIZE;
    q_count++;
    pthread_cond_signal(&q_cond_not_empty);
    pthread_mutex_unlock(&q_mutex);
}

void process_dir(const char *src_base, const char *curr_src, const char *dst_base) {
    DIR *dir = opendir(curr_src);
    if (!dir) return;

    struct dirent *entry;
    char s_full[PATH_MAX];
    char d_full[PATH_MAX];

    // Recursive Mkdir: Ensure destination exists
    if ((CURRENT_MODE == MODE_COPY || CURRENT_MODE == MODE_MOVE) && dst_base) {
        pthread_mutex_lock(&mkdir_mutex);
        mkdirs(dst_base, 0755);
        pthread_mutex_unlock(&mkdir_mutex);
    }

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        snprintf(s_full, sizeof(s_full), "%s/%s", curr_src, entry->d_name);
        struct stat st;
        if (lstat(s_full, &st) == -1) continue;

        if (S_ISDIR(st.st_mode)) {
            if (CURRENT_MODE == MODE_ORGANIZE) {
                process_dir(src_base, s_full, dst_base);
            } else {
                if (dst_base) snprintf(d_full, sizeof(d_full), "%s/%s", dst_base, entry->d_name);
                process_dir(src_base, s_full, dst_base ? d_full : NULL);
            }
        } else {
            if (CURRENT_MODE == MODE_ORGANIZE) enqueue(s_full, dst_base, entry->d_name);
            else {
                if (dst_base) snprintf(d_full, sizeof(d_full), "%s/%s", dst_base, entry->d_name);
                enqueue(s_full, dst_base ? d_full : NULL, NULL);
            }
        }
    }
    closedir(dir);
}

// --- Modified Scan: Includes Live Progress ---
void scan_recursive(const char *path) {
    DIR *dir = opendir(path);
    if (!dir) return;
    struct dirent *entry;
    struct stat st;
    char full[PATH_MAX];
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".")==0 || strcmp(entry->d_name, "..")==0) continue;
        snprintf(full, sizeof(full), "%s/%s", path, entry->d_name);
        if (lstat(full, &st) == -1) continue;
        if (S_ISDIR(st.st_mode)) {
            scan_recursive(full);
        } else {
            atomic_fetch_add(&total_files, 1);
            atomic_fetch_add(&total_bytes, st.st_size);
            if (CURRENT_MODE == MODE_ORGANIZE) get_category(entry->d_name, 1, st.st_size);

            // LIVE UPDATE: Print progress every 50 files
            if (atomic_load(&total_files) % 50 == 0) {
                printf("\r\033[K[SCAN] Found: %lu files | Size: %.2f MB", 
                       atomic_load(&total_files), 
                       (double)atomic_load(&total_bytes)/(1024*1024));
                fflush(stdout);
            }
        }
    }
    closedir(dir);
}

void *monitor_routine(void *arg) {
    unsigned long tf = atomic_load(&total_files);
    unsigned long long tb = atomic_load(&total_bytes);
    printf("\033[?25l"); 

    while (!done_producing || atomic_load(&active_workers) > 0) {
        unsigned long cf = atomic_load(&processed_files);
        unsigned long long cb = atomic_load(&processed_bytes);
        int pct = (tf > 0) ? (int)(((double)cf / tf) * 100.0) : 0;
        if (pct > 100) pct = 100;
        double mb_cur = (double)cb / (1024*1024);
        double mb_tot = (double)tb / (1024*1024);

        if (CURRENT_MODE == MODE_DELETE) 
            printf("\r\033[K[DEL] %lu/%lu (%d%%) | W:%d", cf, tf, pct, atomic_load(&active_workers));
        else 
            printf("\r\033[K[WRK] %.1f/%.1f MB (%d%%) | F:%lu | W:%d", mb_cur, mb_tot, pct, cf, atomic_load(&active_workers));
        
        fflush(stdout);
        usleep(150000);
        if (cf >= tf && done_producing && atomic_load(&active_workers) == 0) break;
    }
    printf("\033[?25h\n");
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <flags> [args]\nTry '%s -h' for help.\n", argv[0], argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "-v") == 0) { printf("DarkFile v%s\n", TOOL_VERSION); return 0; }
    if (strcmp(argv[1], "-h") == 0) {
        printf("DarkFile v%s\nUsage:\n  -c <src> <dst>\n  -m <src> <dst>\n  -d <target>\n  -o [target]\n  --count <target>\n", TOOL_VERSION);
        return 0;
    }
    if (strcmp(argv[1], "--info") == 0) { printf("Tool DarkFile\nVersion: %s\nCompany: %s\nAuthor: %s\n",TOOL_VERSION,TOOL_COMPANY,TOOL_AUTHOR); return 0;}

    char *mode_arg = argv[1];
    char *src = NULL;
    char *raw_dst = NULL;

    if (strcmp(mode_arg, "-o") == 0) {
        CURRENT_MODE = MODE_ORGANIZE;
        if (argc < 3) src = "."; 
        else src = argv[2];
        raw_dst = src;
    }
    else {
        if (argc < 3) { printf("Error: Missing args.\n"); return 1; }
        src = argv[2];
        raw_dst = (argc > 3) ? argv[3] : NULL;
        if (strcmp(mode_arg, "-c") == 0) CURRENT_MODE = MODE_COPY;
        else if (strcmp(mode_arg, "-m") == 0) CURRENT_MODE = MODE_MOVE;
        else if (strcmp(mode_arg, "-d") == 0) CURRENT_MODE = MODE_DELETE;
        else if (strcmp(mode_arg, "--count") == 0) CURRENT_MODE = MODE_COUNT;
        else { printf("Unknown flag.\n"); return 1; }
    }

    // --- SMART PATH RESOLUTION ---
    char final_dst[4096];
    char *dst = NULL;

    if ((CURRENT_MODE == MODE_COPY || CURRENT_MODE == MODE_MOVE) && raw_dst) {
        struct stat st;
        int dst_exists = (stat(raw_dst, &st) == 0 && S_ISDIR(st.st_mode));
        
        // Remove trailing slash from src for basename calculation
        char *clean_src = strdup(src);
        size_t slen = strlen(clean_src);
        if (slen > 1 && clean_src[slen-1] == PATH_SEP) clean_src[slen-1] = 0;
        char *base = basename(clean_src);

        // Check if user provided trailing slash in destination (Implying "Put inside")
        size_t dlen = strlen(raw_dst);
        int has_slash = (dlen > 0 && raw_dst[dlen-1] == PATH_SEP);

        if (dst_exists || has_slash) {
            // If dest exists OR user typed "folder/", we force behavior: dst/src_name
            snprintf(final_dst, sizeof(final_dst), "%s/%s", raw_dst, base);
        } else {
            // Dest doesn't exist and no slash -> Rename/Copy behavior (Standard cp)
            strcpy(final_dst, raw_dst);
        }
        dst = final_dst;
        free(clean_src);
    } 
    else if (raw_dst) {
        strcpy(final_dst, raw_dst);
        dst = final_dst;
    }

    if ((CURRENT_MODE==MODE_COPY || CURRENT_MODE==MODE_MOVE) && is_subdirectory(src, dst)) {
        printf("[ERROR] Recursion: Destination is inside Source.\n"); return 1;
    }

    printf("Scanning %s...\n", src);
    printf("\033[?25l"); // Hide cursor during scan

    scan_recursive(src);
    
    printf("\033[?25h"); // Show cursor after scan
    printf("\n"); // Newline to separate scan progress from summary
    
    if (CURRENT_MODE == MODE_ORGANIZE) {
        printf("\n--- ORGANIZE ANALYSIS ---\n");
        printf("Images:    %lu\nVideos:    %lu\nAudio:     %lu\nDocs:      %lu\nAPKs:      %lu\nArchives:  %lu\nOthers:    %lu\n", 
               count_img, count_vid, count_aud, count_doc, count_apk, count_arc, count_oth);
        printf("-------------------------\n");
    }
    printf("TOTAL: %lu Files (%.2f MB)\n", atomic_load(&total_files), (double)atomic_load(&total_bytes)/(1024*1024));
    
    if (CURRENT_MODE == MODE_COUNT) return 0;

    // --- PERFORMANCE ENGINE ---
    int device_cores = get_cpu_cores();
    int w_lower = 1;
    int w_std   = device_cores;
    int w_high  = device_cores * 2;
    int w_ultra = device_cores * 4;
    
    if (w_ultra > 128) w_ultra = 128; // Safety cap

    printf("\n[DEVICE PERFORMANCE DETECTED: %d Cores]\n", device_cores);
    printf("1. Lower    (1 Thread)   - Background Safe\n");
    printf("2. Standard (%d Threads) - Recommended\n", w_std);
    printf("3. Higher   (%d Threads) - Aggressive\n", w_high);
    printf("4. Ultra    (%d Threads) - RISK! (Max Load)\n", w_ultra);
    printf("Select [1-4]: ");
    
    int choice;
    int workers = w_std;
    if (scanf("%d", &choice) == 1) {
        if (choice == 1) workers = w_lower;
        else if (choice == 3) workers = w_high;
        else if (choice == 4) workers = w_ultra;
        else workers = w_std; 
    }

    printf("\nACTION: %s\nSRC: %s\nDST: %s\nWORKERS: %d\nType 'YES': ", 
           mode_arg, src, dst?dst:"(In-Place)", workers);
    char confirm[10];
    if (scanf("%9s", confirm) != 1 || strcmp(confirm, "YES") != 0) { printf("Aborted.\n"); return 0; }

    pthread_t *th = malloc(sizeof(pthread_t) * workers);
    pthread_t mon;
    for(int i=0; i<workers; i++) pthread_create(&th[i], NULL, worker_routine, NULL);
    pthread_create(&mon, NULL, monitor_routine, NULL);

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    // Initial root handling
    if (CURRENT_MODE == MODE_ORGANIZE) {
        process_dir(src, src, dst);
    } else {
        process_dir(src, src, dst);
    }

    done_producing = 1;
    pthread_cond_broadcast(&q_cond_not_empty);
    for(int i=0; i<workers; i++) pthread_join(th[i], NULL);
    pthread_join(mon, NULL);
    free(th);

    if (CURRENT_MODE == MODE_DELETE || CURRENT_MODE == MODE_MOVE) {
        printf("\nCleaning up empty directories...");
        cleanup_dirs(src);
        rmdir(src);
    }

    clock_gettime(CLOCK_MONOTONIC, &end);
    double t = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
    printf("\nDone in %.2fs.\n", t);
    return 0;
}

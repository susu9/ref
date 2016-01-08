/*
* Copyright (C) Rick Chang <chchang915@gmail.com>
* 
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or (at your option) any later version.
* 
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
* 
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdarg.h>

#define DEFAULT_SHOW_MAX 20
#define DEFAULT_DEL " :\t#()<>,"
#define START_ID 1
#define INI_BUF_LEN 256

#define EMPTY_ID -2

#define RED     "\x1b[31m"
#define GREEN   "\x1b[32m"
#define NONE    "\x1b[0m"

#define FLAG_TRACE  (1 << 0)
#define FLAG_UPDATE (1 << 1)
#define FLAG_NEWID  (1 << 2)

#define LOG_ERR(fmt, args...) printf (RED fmt NONE "\n", ##args)
#define LOG_NOTE(fmt, args...) printf (GREEN fmt NONE "\n", ##args)

const char usage_info[] =
    "usage : ref [ref#] <cmd> [-<row>[,<col>] | -d<row>,<col>]\n"
    "        ref -l [ref#] [-<row>[,<col>] | -d<row>,<col>]\n"
    "        ref -e [ref#] <cmd> [-<row>[,<col>] | -d<row>,<col>]\n"
    "        ref -h [-all | -n <num>]\n"
    "        ref -u [ref#]\n"
    "        ref -t <ref#>\n"
    "        ref -d <ref#> ... | <ref#>-<ref#>\n";

const char version_info[] =
    "1.0-rc";

struct ref {
    int cur_id;
    char *head_path;
    char *his_path;
    char *cwd;
    char *cur_cmd_path;
    char *cur_log_path;
    char *cur_ref_path;
    char *cmd;
    char *del;
    int flag;
    int dir;
};

char *get_str(const char *fmt, ...)
{
    char *str;
    va_list arg;
    int n, s;

    va_start(arg, fmt);
    s = vsnprintf(NULL, 0, fmt, arg);
    va_end(arg);
    if (s <= 0)
        goto err;

    s++;
    str = malloc(s);
    if (!str)
        goto err;

    va_start(arg, fmt);
    n = vsnprintf(str, s, fmt, arg);
    va_end(arg);
    if (n < 0) {
        free(str);
        goto err;
    }
    //printf("%s\n", str);
    return str;
err:
    return NULL;
}

int update_his(struct ref *r)
{
    char *cmd = get_str("cd %s/.ref/cmd; ls -t ref* > %s 2> /dev/null", r->cwd, r->his_path);

    if (!cmd)
        return -1;
    system(cmd);
    free(cmd);
    return 0;
}

int get_refid(char **buf, size_t *len, FILE *f)
{
    int id;

    if (getline(buf, len, f) == -1)
        return -1;
    if (sscanf(*buf, "ref%d\n", &id) != 1)
        return -1;
    return id;
}

int save_cur_head(struct ref *r, int id)
{
    FILE *f;

    f = fopen(r->head_path, "w");
    if (!f)
        return -1;
    fprintf(f, "ref%d\n", id);
    fclose(f);
    if (id != EMPTY_ID)
        printf("switch head to \'ref%d\'\n", id);
    else
        printf("switch head to \'null\'\n");
    return 0;
}

int save_cur_cmd(struct ref *r)
{
    FILE *f = fopen(r->cur_cmd_path, "w");

    if (!f)
        return -1;

    fprintf(f, "%s\n", r->cmd);
    fclose(f);
    return 0;
}


int update_head(struct ref *r)
{
    FILE *f;
    int id;
    size_t len = 0;
    char *buf = NULL;

    f = fopen(r->his_path, "r");
    if (!f)
        return -1;

    id = get_refid(&buf, &len, f);
    if (buf)
        free(buf);
    fclose(f);

    if (id < 0)
        id = EMPTY_ID;
    return save_cur_head(r, id);
}


char *get_esc_end(const char *s)
{
    const char *eset = "HfABCDsuJKmhlp";
    return strpbrk(s, eset);
}
/*
   Only remove Graphics Mode
*/
void rm_esc(char *r)
{
    char *w, *e;

    if (!r || strlen(r) < 2)
        return;

    for (w = r; *r != '\0'; r++) {
        if (*r == 0x1b && *(r + 1) == 0x5b) {
            e = get_esc_end(r + 2);
            if (e) {
                r = e;
                continue;
            }
        }
        *w++ = *r;
    }
    *w = '\0';
}

void show_ref(struct ref *r)
{
    printf("cur id: %d\n", r->cur_id);
    if (r->head_path)
        printf("head:%s\n", r->head_path);
    if (r->his_path)
        printf("his:%s\n", r->his_path);
    if (r->cwd)
        printf("cwd:%s\n", r->cwd);
    if (r->cur_log_path)
        printf("log:%s\n", r->cur_log_path);
    if (r->cur_ref_path)
        printf("ref:%s\n", r->cur_ref_path);
    if (r->cur_cmd_path)
        printf("cmd:%s\n", r->cur_cmd_path);
    if (r->cmd)
        printf("cmd:\"%s\"\n", r->cmd);
    if (r->del)
        printf("del:\"%s\"\n", r->del);
}

int get_offset(const char *ref_path, const int index)
{
    FILE *in = fopen(ref_path, "rb");
    int offset;

    if (!in)
        return -1;

    if(fseek(in, index*sizeof(offset), SEEK_SET)) {
        fclose(in);
        return -1;
    }

    fread(&offset, sizeof(offset), 1, in);
    fclose(in);
    //printf("get pos %d: %d\n", index, offset);
    return offset;
}

int rm_tail(char *buf, char c)
{
    char *t = strrchr(buf, c);
    if (t)
        *t = '\0';
    return 0;
}

char* get_col(struct ref *r, char *buf, int index)
{
    char *tok  = strtok(buf, r->del);

    for (; tok && index; index--) {
        tok = strtok(NULL, r->del);
    }
    return tok;
}

char* get_ref_tok(struct ref *r, int row, int col)
{
    FILE *in;
    int pos;
    char *buf = NULL;
    char *tok;
    size_t len = 0;

    if (row < START_ID)
        return NULL;

    pos = get_offset(r->cur_ref_path, row - START_ID);
    if (pos < 0)
        return NULL;

    in = fopen(r->cur_log_path, "r");
    if (!in)
        return NULL;

    if (fseek(in, pos, SEEK_SET))
        goto err;

    if (getline(&buf, &len, in) == -1)
        goto err;
    //printf("get row %s \n", buf);
    rm_tail(buf, '\n');
    fclose(in);

    if (col < START_ID)
        return buf;

    tok = get_col(r, buf, col - START_ID);
    if (!tok)
        goto err;
    memmove(buf, tok, strlen(tok) + 1);
    //printf("get col %s\n", buf);
    return buf;
err:
    fclose(in);
    if (buf)
        free(buf);
    return NULL;
}

void get_dir(char *s)
{
    char *e = strrchr(s, '/');
    *++e = '\0';
}

char *replace_cmd(struct ref *r, const char* a)
{
    int row = -1;
    int col = -1;
    int ret;
    char *tok;

    if (r->cur_id == EMPTY_ID)
        return NULL;

    if(a[0] == '-') {
        ret = sscanf(a, "-%d,%d", &row, &col);
        if (ret <= 0) {
            if (sscanf(a, "-d%d,%d", &row, &col) != 2)
                return NULL;
            r->dir = 1;
        }
        //printf("%d %d\n", row, col);
        tok = get_ref_tok(r, row, col);

        rm_esc(tok);
        if (r->dir)
            get_dir(tok);
        return tok;
    }
    return NULL;
}

int create_cmd(struct ref *r, char **a)
{
    int i;
    char *cmd;
    int cmd_size = 0;
    int buf_size = INI_BUF_LEN;

    cmd = malloc(buf_size);
    if (!cmd)
        return -1;
    r->cmd = cmd;

    for (i = 0; a[i]; i++) {
        char *buf, *tok;
        int len;

        buf = replace_cmd(r, a[i]);
        tok = buf ? buf : a[i];
        len = strlen(tok);

        while (cmd_size + len + 1 > buf_size) {
            buf_size *= 2;
            cmd = realloc(r->cmd, buf_size);
            if (!cmd) {
                if (buf) 
                    free(buf);
                return -1;
            }
            r->cmd = cmd;
            cmd += cmd_size;
        }

        strncpy(cmd, tok, len);
        cmd += len;
        *cmd++ = ' ';
        cmd_size += (len + 1);
        if (buf)
            free(buf);
    }
    *(--cmd) = '\0';
    return 0;
}

int show_col(struct ref *r, int row, int col)
{
    char *tok = get_ref_tok(r, row, col);

    if (!tok)
        return -1;

    rm_esc(tok);

    if (r->dir)
        get_dir(tok);

    //printf("%d,%d: %s\n", row, col, tok);
    printf("%s\n", tok);
    free(tok);
    return 0;
}

int show_row(struct ref *r, int row)
{
    char *buf = get_ref_tok(r, row, -1);
    char *tok;
    int i;

    if (!buf)
        return -1;

    printf("%d: %s\n", row, buf);
    tok = strtok(buf, r->del);
    for (i = START_ID; tok; i++) {
        printf("|- %d,%d: %s\n", row, i, tok);
        tok = strtok(NULL, r->del);
    }

    free(buf);
    return 0;
}

int show_res(struct ref *r)
{
    FILE *f = fopen(r->cur_log_path, "r");
    size_t len = 0;
    char *buf = NULL; 
    int i;

    if (!f)
        return -1;

    for (i = START_ID; getline(&buf, &len, f) != -1; i++) {
        printf("%5d: %s", i, buf);
    }
    fclose(f);
    if (buf)
        free(buf);
    return 0;
}

int cmd_to_argv(char *cmd, char**argv)
{
    int len = strlen(cmd);
    int argc = 1;
    int i;

    //printf("len %d\n", len); 
    argv[0] = &cmd[0];
    for (i = 0; i < len; i++) {
        //printf("%d = %lx (%c)\n", i, &cmd[i], cmd[i]);
        if (cmd[i] == ' ') {
            cmd[i] = '\0';
            if (cmd[i+1] != '\0') {
                argv[argc] = &cmd[i+1];
                //printf("get argv[%d] = %p\n", argc, argv[argc]);
                argc++;
            }
        }
    }

    return 0;
}

int set_cur_head(struct ref *r, int id)
{
    char *log;
    char *ref;
    char *cmd;

    cmd = get_str("%s/.ref/cmd/ref%d", r->cwd, id);
    if (!cmd)
        return -1;

    log = get_str("%s/.ref/log/ref%d", r->cwd, id);
    if (!log)
        return -1;

    ref = get_str("%s/.ref/ref/ref%d", r->cwd, id);
    if (!ref) {
        free(log);
        return -1;
    }

    if (r->cur_cmd_path)
        free(r->cur_cmd_path);
    if (r->cur_log_path)
        free(r->cur_log_path);
    if (r->cur_ref_path)
        free(r->cur_ref_path);

    r->cur_cmd_path = cmd;
    r->cur_log_path = log;
    r->cur_ref_path = ref;
    if (id != r->cur_id)
        r->flag |= FLAG_NEWID;
    r->cur_id = id;
    return 0;
}

int add_ref(struct ref *r)
{
    FILE *f;
    int id;
    size_t len = 0;
    char *buf = NULL;

    if (r->cur_id == EMPTY_ID)
        return set_cur_head(r, 0);

    f = fopen(r->his_path, "r");
    if (!f)
        return -1;

    id = get_refid(&buf, &len, f);
    fclose(f);
    if (buf)
        free(buf);

    if (id < 0)
        return -1;
    return set_cur_head(r, ++id);
}

int fd_to_ref(struct ref *r, int fd)
{
    FILE *f;
    FILE *log;
    FILE *ref;
    char *buf = NULL;
    size_t len = 0;
    int i, pos;

    f = fdopen(fd, "r");
    if (!f)
        return -1;

    if (!(r->flag & FLAG_UPDATE)) {
        if (add_ref(r))
            return -1;
        if (save_cur_cmd(r))
            return -1;
    }

    log = fopen(r->cur_log_path, "w");
    if (!log)
        return -1;

    ref = fopen(r->cur_ref_path, "wb");
    if (!ref) {
        fclose(ref);
        return -1;
    }

    if (r->flag & FLAG_NEWID)
        save_cur_head(r, r->cur_id);
    update_his(r);

    LOG_NOTE("%s", r->cmd);

    pos = ftell(log);
    for (i = START_ID; getline(&buf, &len, f) != -1; i++) {
        //write(STDOUT_FILENO, buf, len);
        pos = ftell(log);
        fwrite(&pos, sizeof(pos), 1, ref);
        fflush(ref);
        fprintf(log, "%s", buf);
        fflush(log);

        printf("%5d: %s", i, buf);
    }

    fclose(f);
    fclose(log);
    fclose(ref);
    return 0;
}


int get_mtime(const char *path, time_t *t)
{
    struct stat st;

    if (stat(path, &st) == -1)
        return -1;
    *t = st.st_mtime;
    return 0;
}

int get_info_ref(struct ref *r, char *buf)
{
    time_t t;
    struct tm * timeinfo;
    char time[80];

    if (get_mtime(r->cur_log_path, &t))
        return -1;

    timeinfo = localtime (&t);
    strftime(time, 80, "%F %R %a", timeinfo);
    sprintf(buf, "%s - %s\n", time, r->cmd);

    return 0;
}

void init_cmd()
{
    char *v;
#if __APPLE__ || __MACH__
    v = getenv("TERM");
    if (v)
        setenv("CLICOLOR_FORCE", v, 0);
#endif
}

int launch(struct ref *r)
{
    int pipefd[2];
    pid_t pid, wpid;
    int status;
    char* new_argv[64] = {0};

    if (r->flag & FLAG_TRACE)
        pipe(pipefd);

    init_cmd();
    if (!(r->flag & FLAG_TRACE))
        LOG_NOTE("%s", r->cmd);

    pid = fork();
    if (pid == 0) {
    if (r->flag & FLAG_TRACE) {
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[0]);
        //write(STDOUT_FILENO, cmd, strlen(cmd));
        //printf("%s\n", r->cmd);
        //fflush(stdout);
    }
    cmd_to_argv(r->cmd, new_argv);
    if (execvp(new_argv[0], new_argv) == -1) {
        perror(NULL);
        exit(EXIT_FAILURE);
    }
    exit(EXIT_FAILURE);
    } else if (pid > 0){
        if (r->flag & FLAG_TRACE) {
            dup2(pipefd[0], STDIN_FILENO);
            close(pipefd[1]);
            if (fd_to_ref(r, pipefd[0])){
                close(pipefd[0]);
                return -1;
            }
            close(pipefd[0]);
            wpid = waitpid(pid, &status, WUNTRACED);
        }
        else {
            do {
              wpid = waitpid(pid, &status, WUNTRACED);
            } while (!WIFEXITED(status) && !WIFSIGNALED(status));
        }
        return WEXITSTATUS(status);
    }

    perror(NULL);
    return -1;
}

int get_disp_w(int d)
{
    int i = 0;

    do {
        i++;
        d /= 10;
    }while (d);

    return i;
}

int get_ref_info(struct ref *r, int id, char *buf)
{
    char *path = get_str("%s/.ref/log/ref%d", r->cwd, id);
    time_t t;
    struct tm * timeinfo;
    char time[80];

    if (get_mtime(path, &t)) {
        free(path);
        return -1;
    }
    free(path);
    timeinfo = localtime (&t);
    strftime(time, 80, "%F %R %a", timeinfo);
    sprintf(buf, "%s", time);

    return 0;
}

int show_ref_cmd(struct ref *r)
{
    FILE *f = fopen(r->cur_cmd_path, "r");
    char *cmd = NULL;
    size_t len = 0;

    if (!f)
        return -1;

    if (getline(&cmd, &len, f) == -1){
        fclose(f);
        return -1;
    }
    rm_tail(cmd, '\n');

    LOG_NOTE("%s", cmd);
    fclose(f);
    return 0;
}

int get_ref_cmd(struct ref *r, int id, char **buf, size_t *len)
{
    FILE *f;
    char *path = get_str("%s/.ref/cmd/ref%d", r->cwd, id);

    f = fopen(path, "r");
    if (!f)
        return -1;

    if (getline(buf, len, f) == -1){
        fclose(f);
        return -1;
    }

    rm_tail(*buf, '\n');
    fclose(f);
    return 0;
}

int show_ref_his(struct ref *r, int max)
{
    FILE *fh;
    size_t len = 0;
    char *buf = NULL;
    int id, i;
    int disp_w_id = 2;
    char info[256] = "error";
    int ret = -1;

    //update_his(r);
    fh = fopen(r->his_path, "r");
    if (!fh)
        return -1;

    id = get_refid(&buf, &len, fh);
    disp_w_id = get_disp_w(id);

    for (i = 0; id != -1 && i < max; i++) {
        if (get_ref_info(r, id, info))
            goto end;
        if (get_ref_cmd(r, id, &buf, &len))
            goto end;
        if (id == r->cur_id)
            LOG_NOTE("* ref%-*d  %s - %s", disp_w_id, id, info, buf);
        else
            printf("  ref%-*d  %s - %s\n", disp_w_id, id, info, buf);
        id = get_refid(&buf, &len, fh);
    }
    if (id >= r->cur_id) {
        if (get_ref_info(r, r->cur_id, info))
            goto end;
        if (get_ref_cmd(r, r->cur_id, &buf, &len))
            goto end;
        printf("--------------------------------------  \n");
        LOG_NOTE("* ref%-*d  %s - %s", disp_w_id, r->cur_id, info, buf);
    }

    ret = 0;
end:
    if (buf)
        free(buf);
    fclose(fh);
    return ret;
}

char *get_cwd(void)
{
    char *str = NULL;
    int s;
    char *p;

    for (s = INI_BUF_LEN;;s*=2) {
        p = realloc(str, s);
        if (!p)
            goto err;
        str = p;
        if (getcwd(str, s))
            break;
    }
    //printf("%s\n", str);
    return str;
err:
    if (str)
        free(str);
    return NULL;
}

int get_cur_refid(const char *path)
{
    FILE *f = fopen(path, "r");
    int id, n;

    if (!f)
        return EMPTY_ID;

    n = fscanf(f, "ref%d", &id);
    fclose(f);

    if (n <= 0)
        return -1;

    return id;
}

int set_del(struct ref *r, char *c)
{
    int n = strlen(c);
    char *del;

    if (n <= 0)
        return -1;

    del = malloc(n + 1);
    if (!del)
        return -1;
    strncpy(del, c, n);

    if (r->del)
        free(r->del);
    r->del = del;
    printf("new %s\n", del);
    return 0;
}

char *get_del()
{
    char *c = getenv("REF_DEL");
    char *del;
    int n;

    if (c)
        n = strlen(c) + 1;
    else {
        c = DEFAULT_DEL;
        n = sizeof(DEFAULT_DEL);
    }
    del = malloc(n);
    if (!del)
        return NULL;
    sprintf(del, "%s", c);
    return del;
}

void deinit_ref(struct ref *r)
{
    if (r->head_path)
        free(r->head_path);
    if (r->his_path)
        free(r->his_path);
    if (r->cwd)
        free(r->cwd);
    if (r->cur_cmd_path)
        free(r->cur_cmd_path);
    if (r->cur_log_path)
        free(r->cur_log_path);
    if (r->cur_ref_path)
        free(r->cur_ref_path);
    if (r->cmd)
        free(r->cmd);
    if (r->del)
        free(r->del);
}

int first_init(struct ref *r)
{
    char *cmd;

    cmd = get_str("mkdir -p %s/.ref/log %s/.ref/ref %s/.ref/cmd",
            r->cwd, r->cwd, r->cwd);
    if (!cmd)
        return -1;
    system(cmd);
    free(cmd);
    return 0;
}

int init_ref(struct ref *r)
{
    memset(r, 0, sizeof(*r));

    r->cwd = get_cwd();
    if (!r->cwd)
        goto err;

    r->del = get_del();
    if (!r->del)
        goto err;

    r->head_path = get_str("%s/.ref/head", r->cwd);
    if (!r->head_path)
        goto err;

    r->his_path = get_str("%s/.ref/his", r->cwd);
    if (!r->his_path)
        goto err;

    r->cur_id = get_cur_refid(r->head_path);
    if (r->cur_id == EMPTY_ID)
        return first_init(r);
    if (r->cur_id < 0)
        goto err;

    //show_ref(r);
    return 0;
err:
    deinit_ref(r);
    return -1;
}

char* get_cur_cmd (struct ref *r)
{
    FILE *f = fopen(r->cur_cmd_path, "r");
    char *cmd = NULL;
    size_t len = 0;

    if (!f)
        return NULL;

    if (getline(&cmd, &len, f) == -1) {
        fclose(f);
        if (cmd)
            free(cmd);
        return NULL;
    }

    fclose(f);
    rm_tail(cmd, '\n');
    return cmd;
}

int rm_cur_ref (struct ref *r, int id)
{
    char *cmd = get_str("cd %s/.ref; rm -f log/ref%d; rm -f ref/ref%d; rm -f cmd/ref%d",
            r->cwd, id, id, id);

    if (!cmd)
        return -1;
    system(cmd);
    free(cmd);
    return 0;
}

int ref_is_exist(struct ref *r, int id)
{
    char *path = get_str("%s/.ref/log/ref%d", r->cwd, id);
    FILE *f;

    if (!path)
        return 0;

    f = fopen(path, "r");
    if (f) {
        fclose(f);
        free(path);
        return 1;
    }
    else
        return 0;
}

void usage(void)
{
    printf("%s", usage_info);
}

void version(void)
{
    printf("%s\n", version_info);
}

int main (int argc, char **argv)
{
    int row = -1, col = -1;
    int ret = -1;
    struct ref r;
    int id = -1, max = -1;

    if (argc < 2) {
        usage();
        return -1;
    }

    if (strcmp("-v", argv[1]) == 0 || strcmp("--version", argv[1]) == 0) {
        version();
        return 0;
    }

    if (init_ref(&r))
        return -1;

    if (strcmp("-e", argv[1]) == 0 || strcmp("--exec", argv[1]) == 0) {
        int i = 2;
        int id = r.cur_id;

        if (argc < 3)
            goto end;

        if (sscanf(argv[i], "ref%d", &id) == 1)
            i++;
        /*
        if (argc > i + 1 && strcmp("-del", argv[i]) == 0) {
            set_del(&r, argv[i+1]);
            i += 2;
        }
        */
        set_cur_head(&r, id);
        create_cmd(&r, &argv[i]);
        ret = launch(&r);
    }
    else if (strcmp("-l", argv[1]) == 0 || strcmp("--log", argv[1]) == 0) {
        int i = 2;

        id = r.cur_id;
        if (argc > 2) {
            /*
            if (argc > i + 1 && strcmp("-del", argv[i]) == 0) {
                set_del(&r, argv[i+1]);
                i += 2;
            }
            */
            if (sscanf(argv[2], "ref%d", &id) == 1){
                if (!ref_is_exist(&r, id)) {
                    LOG_ERR("invalid \'ref%d\'", id);
                    goto end;
                }
                i++;
            }

            if (argc > i) {
                if (sscanf(argv[i], "-%d,%d", &row, &col) > 0)
                    ;
                else if (sscanf(argv[i], "-d%d,%d", &row, &col) == 2)
                    r.dir = 1;
                else
                    goto end;
            }
        }

        set_cur_head(&r, id);
        if (row == -1 && col == -1) {
            show_ref_cmd(&r);
            ret = show_res(&r);
            goto end;
        }
        if (row >= START_ID && col == -1) {
            ret = show_row(&r, row);
            goto end;
        }
        if (row >= START_ID && col >= START_ID) {
            ret = show_col(&r, row, col);
            goto end;
        }
    }
    else if (strcmp("-h", argv[1]) == 0 || strcmp("--history", argv[1]) == 0) {
        if (argc == 2)
            max = DEFAULT_SHOW_MAX;
        else if (argc == 3 && strcmp(argv[2], "-all") == 0)
            max = -2;
        else if (argc == 4 && strcmp(argv[2], "-n") == 0 && 
                (sscanf(argv[3], "%d", &max) == 1)) {
            if (max < 0) 
                goto end;
        }
        else
            goto end;
        ret = show_ref_his(&r, max);
    }
    else if (strcmp("-t", argv[1]) == 0 || strcmp("--to", argv[1]) == 0) {
        if (argc != 3)
            goto end;
        if (sscanf(argv[2], "ref%d", &id) != 1)
            goto end;
        if (!ref_is_exist(&r,id)) {
            LOG_ERR("invalid \'ref%d\'", id);
            goto end;
        }
        if (id != r.cur_id)
            ret = save_cur_head(&r, id);
        else
            ret = 0;
    }
    else if (strcmp("-d", argv[1]) == 0 || strcmp("--delete", argv[1]) == 0) {
        int i, s, e;
        int newid = 0;

        if (argc < 3)
            goto end;

        if (sscanf(argv[2], "ref%d-ref%d", &s, &e) == 2) {

            if (s > e) {
                int t = s;
                s = e;
                e = t;
            }
            for (id = s; id <= e; id++) {
                if (ref_is_exist(&r, id)) {
                    if (r.cur_id == id)
                        newid = 1;
                    rm_cur_ref(&r, id);
                    printf("ref%d has been removed.\n", id);
                }
            }
        }
        else {
            for (i = 2; i < argc; i++) {
                if (sscanf(argv[i], "ref%d", &id) == 1 && ref_is_exist(&r, id)) {
                    if (r.cur_id == id)
                        newid = 1;
                    rm_cur_ref(&r, id);
                    printf("ref%d has been removed.\n", id);
                }
                else
                    LOG_ERR("invalid \'%s\'", argv[i]);
            }
        }

        update_his(&r);
        if (newid)
            update_head(&r);
    }
    else if (strcmp("-u", argv[1]) == 0 || strcmp("--update", argv[1]) == 0) {
        id = r.cur_id;
        if (argc == 3) {
            if (sscanf(argv[2], "ref%d", &id) != 1)
                goto end;
            if (!ref_is_exist(&r, id)) {
                LOG_ERR("invalid \'ref%d\'", id);
                goto end;
            }
        }
        else if (r.cur_id == EMPTY_ID)
            goto end;

        r.flag |= (FLAG_TRACE | FLAG_UPDATE);
        set_cur_head(&r, id);
        r.cmd = get_cur_cmd(&r);
        if (!r.cmd)
            goto end;
        ret = launch(&r);
    }
    else {
        int i = 1;
        int id = r.cur_id;

        if (sscanf(argv[i], "ref%d", &id) == 1)
            i++;
        /*
        if (argc > i + 1 && strcmp("-del", argv[i]) == 0) {
            set_del(&r, argv[i+1]);
            i += 2;
        }
        */
        if (isatty(STDOUT_FILENO))
            r.flag |= FLAG_TRACE;
        set_cur_head(&r, id);
        create_cmd(&r, &argv[i]);
        ret = launch(&r);
    }
end:
    deinit_ref(&r);
    return ret;
}


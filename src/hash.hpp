#include <cstdint>
#include <cctype>

uint32_t str_to_hash(const char *in_str, uint32_t hash_a, uint32_t hash_b)
{
  uint32_t i = 0;
  uint32_t a = hash_a;
  uint32_t b = hash_b;

  if (in_str != nullptr)
  {
    while (*in_str != '\0')
    {
      char letter = toupper(*in_str);
      i = (a * i + letter) % 0x7fffffff;
      a = (a * b) % 0x7ffffffe;
      in_str++;
    }
  }
  return i;
}

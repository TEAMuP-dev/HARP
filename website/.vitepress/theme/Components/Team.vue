<script setup>
  import { VPTeamMembers } from 'vitepress/theme'
  import { ref, onMounted } from 'vue'

  const owner = 'TEAMuP-dev';
  const repo = 'HARP';

  const apiUrl = `https://api.github.com/repos/${owner}/${repo}/contributors`;
  const members = ref([]);

  async function fetchContributors() {
    try {
      const response = await fetch(apiUrl);
      const contributors = await response.json();
      // {
      //   "login": "xribene",
      //   "id": 15819935,
      //   "node_id": "MDQ6VXNlcjE1ODE5OTM1",
      //   "avatar_url": "https://avatars.githubusercontent.com/u/15819935?v=4",
      //   "gravatar_id": "",
      //   "url": "https://api.github.com/users/xribene",
      //   "html_url": "https://github.com/xribene",
      //   "followers_url": "https://api.github.com/users/xribene/followers",
      //   "following_url": "https://api.github.com/users/xribene/following{/other_user}",
      //   "gists_url": "https://api.github.com/users/xribene/gists{/gist_id}",
      //   "starred_url": "https://api.github.com/users/xribene/starred{/owner}{/repo}",
      //   "subscriptions_url": "https://api.github.com/users/xribene/subscriptions",
      //   "organizations_url": "https://api.github.com/users/xribene/orgs",
      //   "repos_url": "https://api.github.com/users/xribene/repos",
      //   "events_url": "https://api.github.com/users/xribene/events{/privacy}",
      //   "received_events_url": "https://api.github.com/users/xribene/received_events",
      //   "type": "User",
      //   "site_admin": false,
      //   "contributions": 33
      // },
      members.value = contributors.map((githubUser) => ({
        avatar: githubUser.avatar_url,
        name: githubUser.login,
        title: 'optional title',
        desc: 'optional description',
        links: [{ icon: 'github', link: githubUser.html_url }],
      }));
    } catch (error) {
      console.error('Error fetching contributors:', error);
    }
  }

  onMounted(async () => {
    await fetchContributors();
  });

</script>

<template>
  <div class="content">
    <VPTeamMembers size="small" :members="members" />
  </div>
</template>